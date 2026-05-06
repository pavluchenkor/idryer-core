#!/usr/bin/env python3
"""
gen_uart_protocol_h.py — генератор C++ header'а UART-протокола из mqtt_contract.yaml.

Что генерирует:
  1. UART-константы (из rules.*).
  2. Все enums (из enums.*) — `enum class : uint8_t`.
  3. UartMsgKind — собран из kind_id во всех uart_only + messages.bindings.uart.
  4. Payload struct'ы (из payloads.*) с `#pragma pack(push, 1)` / `__attribute__((packed))`.
  5. `static_assert(sizeof(X) == N, "...")` для каждой struct'ы (N считается validator-логикой).

Output:
  contracts/_generated/uart_protocol.h

Этот файл НЕ должен редактироваться руками. Источник правды — mqtt_contract.yaml.
"""

from __future__ import annotations
import sys
import re
import datetime
from pathlib import Path
import yaml

# Reuse sizeof estimator from validator
sys.path.insert(0, str(Path(__file__).parent))
from validate_contract import sum_fields_size, normalize_kind_id, PRIMITIVE_SIZES, ARRAY_RE, NESTED_ARRAY_RE  # type: ignore


# ── Type mapping yaml → C++ ──────────────────────────────────────────

CPP_PRIMITIVE = {
    "uint8":  "uint8_t",
    "int8":   "int8_t",
    "uint16": "uint16_t",
    "int16":  "int16_t",
    "uint32": "uint32_t",
    "int32":  "int32_t",
    "uint64": "uint64_t",
    "int64":  "int64_t",
    "float":  "float",
    "double": "double",
    "bool":   "bool",
}


def cpp_type(field_spec, field_name: str, enums: dict, payloads: dict) -> str:
    """Возвращает C++-объявление поля (без имени переменной)."""
    if isinstance(field_spec, str):
        return f"/* TODO: free-form '{field_spec}' */ uint8_t /*{field_name}*/[1]"
    if not isinstance(field_spec, dict):
        return f"/* unknown shape */ uint8_t /*{field_name}*/[1]"

    t = field_spec.get("type")
    if t is None:
        return f"/* no type */ uint8_t /*{field_name}*/[1]"

    t = str(t)

    # Primitive array: uint8[N], char[N], uint16[N]
    m = ARRAY_RE.match(t)
    if m:
        basetype = t.split("[")[0]
        n = m.group(1)
        cpp_base = "char" if basetype == "char" else CPP_PRIMITIVE.get(basetype, basetype)
        return f"{cpp_base}    {field_name}[{n}]"

    # Nested struct array: "UartUnitConfig[4]"
    m = NESTED_ARRAY_RE.match(t)
    if m:
        struct_name = m.group(1)
        n = m.group(2)
        return f"{struct_name} {field_name}[{n}]"

    # Primitive scalar with optional count
    if t in PRIMITIVE_SIZES:
        cpp_base = CPP_PRIMITIVE[t]
        count = field_spec.get("count")
        if count:
            return f"{cpp_base}    {field_name}[{count}]"
        return f"{cpp_base}    {field_name}"

    # Enum reference
    if t == "enum":
        ref = field_spec.get("ref")
        if ref:
            return f"{ref} {field_name}"
        return f"uint8_t  {field_name} /* unknown enum */"

    # char[N] form should have been caught by ARRAY_RE; if we hit here, fallback
    if t == "char":
        count = field_spec.get("count")
        if count:
            return f"char     {field_name}[{count}]"
        return f"char     {field_name}"

    # Reference to a struct from payloads
    if t in payloads:
        return f"{t} {field_name}"

    # Unknown type — emit comment
    return f"/* TYPE? '{t}' */ uint8_t {field_name}[1]"


# ── Enum rendering ───────────────────────────────────────────────────


def render_enum(name: str, definition) -> str:
    """Рендерит `enum class Name : uint8_t { ... };` из yaml-определения."""
    out = []

    # Form 1: list of strings (just names, no numbers)
    if isinstance(definition, list):
        out.append(f"enum class {name} : uint8_t {{")
        for i, v in enumerate(definition):
            out.append(f"    {v}    = {i},")
        out.append("};")
        return "\n".join(out)

    # Form 2: dict with name → number
    if isinstance(definition, dict):
        # Detect cpp_enum/json_string_form structure
        if "cpp_enum" in definition:
            cpp = definition["cpp_enum"]
            values = cpp.get("values", [])
            out.append(f"enum class {name} : uint8_t {{")
            if isinstance(values, list):
                for i, v in enumerate(values):
                    out.append(f"    {v:<14} = {i},")
            elif isinstance(values, dict):
                # Sort by numeric value to keep output deterministic
                items = sorted(values.items(), key=lambda kv: kv[1])
                for k, v in items:
                    out.append(f"    {k:<14} = 0x{v:02X},")
            out.append("};")
            return "\n".join(out)

        # Plain numeric mapping {Name: int}
        if all(isinstance(v, int) for v in definition.values()):
            out.append(f"enum class {name} : uint8_t {{")
            items = sorted(definition.items(), key=lambda kv: kv[1])
            for k, v in items:
                out.append(f"    {k:<14} = 0x{v:02X},")
            out.append("};")
            return "\n".join(out)

    return f"// TODO: unsupported enum shape for '{name}'"


# ── Payload struct rendering ─────────────────────────────────────────

# Поля которые мы не выводим в C-struct — это контекстные/мета-поля yaml.
SKIP_FIELDS = {"_comment", "_note", "notes"}


def collect_inline_entry_structs(payloads: dict) -> list[tuple[str, dict, int | None]]:
    """Сканирует все payloads, находит поля с типом 'TypeName[N]' + item_fields,
    и возвращает список (entry_struct_name, item_fields_dict, computed_size_per_item).
    Дедуплицирует по имени.
    """
    seen: dict[str, tuple[dict, int | None]] = {}
    for pname, pdef in payloads.items():
        fields = pdef.get("fields", {}) or {}
        for fname, fspec in fields.items():
            if not isinstance(fspec, dict):
                continue
            t = fspec.get("type")
            if not isinstance(t, str):
                continue
            m = NESTED_ARRAY_RE.match(t)
            if not m:
                continue
            entry_type = m.group(1)
            # Skip if entry_type already defined as a top-level payload
            if entry_type in payloads:
                continue
            item_fields = fspec.get("item_fields")
            if not item_fields:
                continue
            # Compute per-entry size
            per_item = sum_fields_size(item_fields, {}, payloads)
            seen[entry_type] = (item_fields, per_item)
    return [(name, fdict, sz) for name, (fdict, sz) in seen.items()]


def render_entry_struct(name: str, item_fields: dict, per_size: int | None, enums: dict, payloads: dict) -> str:
    """Рендерит вспомогательную struct'у для UartXxxEntry."""
    out = [f"/// Entry-record для inline-массивов в payload'ах."]
    out.append(f"struct {name} {{")
    for fname, fspec in item_fields.items():
        if fname in SKIP_FIELDS:
            continue
        decl = cpp_type(fspec, fname, enums, payloads)
        note = ""
        if isinstance(fspec, dict):
            n = fspec.get("note")
            if n:
                note = "  ///< " + str(n).split('.')[0][:80]
        out.append(f"    {decl};{note}")
    out.append("} __attribute__((packed));")
    if per_size is not None:
        out.append(
            f"static_assert(sizeof({name}) == {per_size}, "
            f'"{name} must be {per_size} bytes (yaml-computed)");'
        )
    return "\n".join(out)


def render_payload(name: str, definition: dict, enums: dict, payloads: dict, computed_size: int | None) -> str:
    fields = definition.get("fields", {})
    if not isinstance(fields, dict):
        return f"// TODO: payload '{name}' has no fields block"

    out = []
    desc = definition.get("description", "").strip()
    if desc:
        for line in desc.splitlines():
            out.append(f"/// {line}".rstrip())
    out.append(f"struct {name} {{")
    for fname, fspec in fields.items():
        if fname in SKIP_FIELDS:
            continue
        decl = cpp_type(fspec, fname, enums, payloads)
        # Inline note as comment
        note = ""
        if isinstance(fspec, dict):
            n = fspec.get("note")
            if n:
                note = "  ///< " + str(n).split('.')[0][:80]
        out.append(f"    {decl};{note}")
    out.append("} __attribute__((packed));")
    if computed_size is not None:
        out.append(
            f"static_assert(sizeof({name}) == {computed_size}, "
            f'"{name} must be {computed_size} bytes (yaml-computed)");'
        )
    return "\n".join(out)


# ── Collect UART message kinds ───────────────────────────────────────


def collect_kinds(doc: dict) -> dict[str, int]:
    """Собирает {KindName: kind_id} из uart_only + bindings.uart.kind."""
    kinds: dict[str, int] = {}
    # uart_only
    for u in doc.get("uart_only", []):
        kid = normalize_kind_id(u.get("kind_id"))
        kind = u.get("kind")
        if kid is not None and kind:
            # keep first occurrence (Command может повторяться с разными cmd_code)
            if kind not in kinds:
                kinds[kind] = kid
    # messages.bindings.uart
    for m in doc.get("messages", []):
        bindings = m.get("bindings") or {}
        uart = bindings.get("uart") or {}
        kid = normalize_kind_id(uart.get("kind_id"))
        kind = uart.get("kind")
        if kid is not None and kind:
            if kind not in kinds:
                kinds[kind] = kid
    return kinds


# ── Constants from rules ─────────────────────────────────────────────


def render_rules(rules: dict) -> str:
    """Из rules.uart_* → constexpr-константы."""
    if not isinstance(rules, dict):
        return ""
    mapping = [
        ("uart_baud",                  "uint32_t", "UART_BAUD"),
        ("uart_protocol_version",      "uint8_t",  "UART_PROTOCOL_VER"),
        ("uart_max_payload_bytes",     "uint8_t",  "UART_MAX_PAYLOAD"),
        ("uart_max_retries",           "uint8_t",  "UART_MAX_RETRIES"),
        ("uart_cmd_timeout_ms",        "uint16_t", "UART_CMD_TIMEOUT_MS"),
        ("uart_heartbeat_interval_ms", "uint16_t", "UART_HEARTBEAT_MS"),
        ("uart_link_loss_ms",          "uint32_t", "UART_LINK_LOSS_MS"),
    ]
    lines = []
    for yaml_key, cpp_type_name, c_name in mapping:
        v = rules.get(yaml_key)
        if v is not None:
            lines.append(f"constexpr {cpp_type_name:<10} {c_name:<22} = {v};")
    return "\n".join(lines)


# ── Main render ──────────────────────────────────────────────────────


def render_header(doc: dict) -> str:
    enums = doc.get("enums") or {}
    payloads = doc.get("payloads") or {}
    rules = doc.get("rules") or {}
    today = datetime.date.today().isoformat()
    yaml_version = doc.get("version", "?")

    out = []
    out.append("// ============================================================================")
    out.append("// AUTO-GENERATED by contracts/gen_uart_protocol_h.py from mqtt_contract.yaml")
    out.append("// DO NOT EDIT — изменения вносятся в yaml, потом регенерация.")
    out.append("//")
    out.append(f"// Generated: {today}")
    out.append(f"// Yaml version: {yaml_version}")
    out.append("// Tool: validate_contract.py + gen_uart_protocol_h.py")
    out.append("// ============================================================================")
    out.append("")
    out.append("#pragma once")
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("namespace idryer {")
    out.append("")

    # Constants
    rules_block = render_rules(rules)
    if rules_block:
        out.append("// ── UART transport constants (из rules.*) ──────────────────────────")
        out.append(rules_block)
        out.append("")

    # Enums
    out.append("// ── Enums ──────────────────────────────────────────────────────────")
    for ename, edef in enums.items():
        out.append(render_enum(ename, edef))
        out.append("")

    # UartMsgKind (composed from kind_ids)
    kinds = collect_kinds(doc)
    if kinds:
        out.append("// ── UART message kinds (kind_id из uart_only + messages) ──────────")
        out.append("enum class UartMsgKind : uint8_t {")
        for kname, kid in sorted(kinds.items(), key=lambda kv: kv[1]):
            out.append(f"    {kname:<18} = 0x{kid:02X},")
        out.append("};")
        out.append("")

    # Payload structs (packed)
    out.append("// ── Payload structs (packed binary layout) ────────────────────────")
    out.append("#pragma pack(push, 1)")
    out.append("")

    # Сначала auxiliary entry-structs (UartTelemetryEntry, UartStatusEntry, UartWeightEntry, etc.)
    # — они referenced через 'TypeName[N]' с inline item_fields в parent payload'ах.
    entry_structs = collect_inline_entry_structs(payloads)
    if entry_structs:
        out.append("// — Auxiliary entry-structs (для inline массивов в parent payload'ах) —")
        for ename, efields, esize in entry_structs:
            out.append(render_entry_struct(ename, efields, esize, enums, payloads))
            out.append("")

    # Затем сами payloads
    for pname, pdef in payloads.items():
        size = sum_fields_size(pdef.get("fields", {}), enums, payloads)
        status = pdef.get("status")
        if status == "legacy_broken":
            out.append(f"// LEGACY/BROKEN: {pname} — см. yaml notes; не использовать в новом коде.")
        out.append(render_payload(pname, pdef, enums, payloads, size))
        out.append("")
    out.append("#pragma pack(pop)")
    out.append("")
    out.append("} // namespace idryer")
    out.append("")

    return "\n".join(out)


def main():
    here = Path(__file__).parent
    yaml_path = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "mqtt_contract.yaml"
    out_dir = here / "_generated"
    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / "uart_protocol.h"

    if not yaml_path.exists():
        print(f"ERROR: yaml not found: {yaml_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Loading: {yaml_path}")
    with yaml_path.open(encoding="utf-8") as f:
        doc = yaml.safe_load(f)

    header = render_header(doc)

    out_path.write_text(header, encoding="utf-8")
    line_count = header.count("\n") + 1
    print(f"Generated: {out_path} ({line_count} lines)")

    # Quick stats
    enums = doc.get("enums") or {}
    payloads = doc.get("payloads") or {}
    kinds = collect_kinds(doc)
    print()
    print(f"  Enums emitted:        {len(enums)}")
    print(f"  Payloads emitted:     {len(payloads)}")
    print(f"  UART kinds emitted:   {len(kinds)}")
    print()
    print("Compare with existing header:")
    print(f"  diff -u lib/idryer-core/src/uart/uart_protocol.h {out_path.relative_to(here.parent.parent.parent.parent)}")


if __name__ == "__main__":
    main()
