#!/usr/bin/env python3
"""
gen_ts_types.py — генератор TypeScript-типов для portal из mqtt_contract.yaml.

Что генерирует:
  1. Enums (string union types, либо numeric — что точнее отражает wire JSON форму).
  2. Payload interfaces (struct → TS interface) с маппингом C-типов в TS:
      - uintN/intN/float → number
      - char[N]          → string
      - char[]/array of  → typed array
      - enum ref         → name of TS enum
      - struct ref       → name of TS interface
  3. Topic constants (suffix → constant) — basic скелет.

Output:
  contracts/_generated/mqtt-api.types.ts

Этот файл НЕ должен редактироваться руками. Источник правды — mqtt_contract.yaml.
"""

from __future__ import annotations
import sys
import re
import datetime
from pathlib import Path
import yaml

sys.path.insert(0, str(Path(__file__).parent))
from validate_contract import sum_fields_size, ARRAY_RE, NESTED_ARRAY_RE  # type: ignore


# ── Type mapping yaml → TS ──────────────────────────────────────────

NUMERIC_TYPES = {
    "uint8", "int8", "uint16", "int16",
    "uint32", "int32", "uint64", "int64",
    "float", "double",
}


def ts_type(field_spec, enums: dict, payloads: dict) -> str:
    """yaml field spec → TS type expression."""
    if isinstance(field_spec, str):
        return "any /* TODO: free-form */"
    if not isinstance(field_spec, dict):
        return "any"

    t = field_spec.get("type")
    if t is None:
        return "unknown"

    t = str(t)

    # Primitive arrays: uint8[N], char[N]
    m = ARRAY_RE.match(t)
    if m:
        basetype = t.split("[")[0]
        if basetype == "char":
            return "string"          # char[N] → string in TS land
        return "number[]"            # uint8[N] etc

    # Nested struct array: "UartUnitConfig[4]"
    m = NESTED_ARRAY_RE.match(t)
    if m:
        struct_name = m.group(1)
        return f"{struct_name}[]"

    # Primitives → number
    if t in NUMERIC_TYPES:
        count = field_spec.get("count")
        if count:
            return "number[]"
        return "number"

    if t == "bool":
        return "boolean"

    if t == "string":
        return "string"

    # Enum reference
    if t == "enum":
        ref = field_spec.get("ref")
        if ref:
            return ref
        return "number"

    # Struct reference (payload name)
    if t in payloads:
        return t

    # Otherwise: unknown — leave as TODO
    return f"any /* TODO: unknown type '{t}' */"


# ── Enum rendering ──────────────────────────────────────────────────


def is_valid_ts_identifier(s: str) -> bool:
    return bool(re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", s))


def render_enum(name: str, definition) -> str:
    """yaml enum → TS const union or numeric enum."""
    out = []

    # List of strings → string union
    if isinstance(definition, list):
        items = " | ".join(f'"{v}"' for v in definition)
        out.append(f"export type {name} = {items};")
        return "\n".join(out)

    if isinstance(definition, dict):
        # Прибор с cpp_enum + json_string_form: предпочитаем string union (то что реально в JSON)
        if "json_string_form" in definition:
            mapping = definition["json_string_form"].get("mapping", {})
            if mapping:
                values = sorted(set(mapping.values()))
                items = " | ".join(f'"{v}"' for v in values)
                out.append(f"export type {name} = {items};")
                # Бонус: numeric enum для UART side
                if "cpp_enum" in definition:
                    cpp_values = definition["cpp_enum"].get("values", {})
                    if isinstance(cpp_values, dict):
                        out.append(f"export const {name}Numeric = {{")
                        items_sorted = sorted(cpp_values.items(), key=lambda kv: kv[1])
                        for k, v in items_sorted:
                            out.append(f"  {k}: {v},")
                        out.append("} as const;")
                return "\n".join(out)

        # Если только cpp_enum (без json формы) — рендерим как TS enum
        if "cpp_enum" in definition:
            values = definition["cpp_enum"].get("values", [])
            out.append(f"export enum {name} {{")
            if isinstance(values, list):
                for i, v in enumerate(values):
                    out.append(f"  {v} = {i},")
            elif isinstance(values, dict):
                items = sorted(values.items(), key=lambda kv: kv[1])
                for k, v in items:
                    out.append(f"  {k} = {v},")
            out.append("}")
            return "\n".join(out)

        # Plain numeric mapping {Name: int}
        if all(isinstance(v, int) for v in definition.values()):
            out.append(f"export enum {name} {{")
            items = sorted(definition.items(), key=lambda kv: kv[1])
            for k, v in items:
                out.append(f"  {k} = {v},")
            out.append("}")
            return "\n".join(out)

    return f"// TODO: unsupported enum shape for '{name}'"


# ── Payload interface rendering ─────────────────────────────────────


SKIP_FIELDS = {"_comment", "_note", "notes"}


def render_payload(name: str, definition: dict, enums: dict, payloads: dict) -> str:
    fields = definition.get("fields", {})
    if not isinstance(fields, dict):
        return f"// TODO: payload '{name}' has no fields block"

    out = []
    desc = definition.get("description", "").strip()
    if desc:
        lines = desc.splitlines()
        if len(lines) == 1:
            out.append(f"/** {lines[0]} */")
        else:
            out.append("/**")
            for line in lines:
                out.append(f" * {line}".rstrip())
            out.append(" */")
    out.append(f"export interface {name} {{")
    for fname, fspec in fields.items():
        if fname in SKIP_FIELDS:
            continue
        ts = ts_type(fspec, enums, payloads)
        # Optional? required?
        is_optional = isinstance(fspec, dict) and fspec.get("optional") is True
        opt = "?" if is_optional else ""
        # Note as JSDoc
        note = ""
        if isinstance(fspec, dict):
            n = fspec.get("note")
            if n:
                note = f"  /** {str(n)[:80]} */"
        out.append(f"  {fname}{opt}: {ts};{note}")
    out.append("}")
    return "\n".join(out)


# ── Auxiliary entry-structs (UartTelemetryEntry, etc.) ──────────────


def collect_inline_entry_structs(payloads: dict) -> list[tuple[str, dict]]:
    seen: dict[str, dict] = {}
    for pname, pdef in payloads.items():
        for fname, fspec in (pdef.get("fields") or {}).items():
            if not isinstance(fspec, dict):
                continue
            t = fspec.get("type")
            if not isinstance(t, str):
                continue
            m = NESTED_ARRAY_RE.match(t)
            if not m:
                continue
            entry_type = m.group(1)
            if entry_type in payloads:
                continue
            item_fields = fspec.get("item_fields")
            if item_fields and entry_type not in seen:
                seen[entry_type] = item_fields
    return list(seen.items())


def render_entry_interface(name: str, item_fields: dict, enums: dict, payloads: dict) -> str:
    out = [f"/** Entry-record для inline-массивов в payload'ах. */"]
    out.append(f"export interface {name} {{")
    for fname, fspec in item_fields.items():
        if fname in SKIP_FIELDS:
            continue
        ts = ts_type(fspec, enums, payloads)
        is_optional = isinstance(fspec, dict) and fspec.get("optional") is True
        opt = "?" if is_optional else ""
        out.append(f"  {fname}{opt}: {ts};")
    out.append("}")
    return "\n".join(out)


# ── Topic constants ─────────────────────────────────────────────────


def render_topic_constants(doc: dict) -> str:
    out = []
    out.append("export const Topics = {")
    out.append("  /** device → backend */")
    for m in doc.get("messages", []):
        bindings = m.get("bindings") or {}
        mqtt = bindings.get("mqtt") or {}
        suffix = mqtt.get("suffix")
        if not suffix:
            continue
        # Generate identifier from name
        name = m.get("name", "").upper().replace(".", "_")
        if not name or not is_valid_ts_identifier(name.lower()):
            continue
        topic_template = mqtt.get("full_topic", f"idryer/{{serial}}/{suffix}")
        out.append(f"  {name}: (serial: string) => `{topic_template.replace('{serial}', '${serial}')}`,")
    out.append("")
    out.append("  /** backend → device (mqtt_only) */")
    for u in doc.get("mqtt_only", []):
        suffix = u.get("suffix")
        if not suffix:
            continue
        name = re.sub(r"[^a-zA-Z0-9]", "_", suffix).upper()
        if not name:
            continue
        topic_template = u.get("full_topic", f"idryer/{{serial}}/{suffix}")
        out.append(f"  {name}: (serial: string) => `{topic_template.replace('{serial}', '${serial}')}`,")
    out.append("} as const;")
    return "\n".join(out)


# ── Main render ─────────────────────────────────────────────────────


def render_module(doc: dict) -> str:
    enums = doc.get("enums") or {}
    payloads = doc.get("payloads") or {}
    today = datetime.date.today().isoformat()
    yaml_version = doc.get("version", "?")

    out = []
    out.append("// ============================================================================")
    out.append("// AUTO-GENERATED by contracts/gen_ts_types.py from mqtt_contract.yaml")
    out.append("// DO NOT EDIT — изменения вносятся в yaml, потом регенерация.")
    out.append("//")
    out.append(f"// Generated: {today}")
    out.append(f"// Yaml version: {yaml_version}")
    out.append("// ============================================================================")
    out.append("")
    out.append("/* eslint-disable */")
    out.append("/* tslint:disable */")
    out.append("")

    # Enums
    out.append("// ── Enums ─────────────────────────────────────────────────────────")
    out.append("")
    for ename, edef in enums.items():
        out.append(render_enum(ename, edef))
        out.append("")

    # Auxiliary entry-structs (UartTelemetryEntry etc.)
    entry_structs = collect_inline_entry_structs(payloads)
    if entry_structs:
        out.append("// ── Auxiliary entry-records ───────────────────────────────────────")
        out.append("")
        for ename, efields in entry_structs:
            out.append(render_entry_interface(ename, efields, enums, payloads))
            out.append("")

    # Payload interfaces
    out.append("// ── Payload interfaces (wire shapes) ──────────────────────────────")
    out.append("")
    for pname, pdef in payloads.items():
        out.append(render_payload(pname, pdef, enums, payloads))
        out.append("")

    # Topic constants
    out.append("// ── Topic helpers ─────────────────────────────────────────────────")
    out.append("")
    out.append(render_topic_constants(doc))
    out.append("")

    return "\n".join(out)


def main():
    here = Path(__file__).parent
    yaml_path = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "mqtt_contract.yaml"
    out_dir = here / "_generated"
    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / "mqtt-api.types.ts"

    if not yaml_path.exists():
        print(f"ERROR: yaml not found: {yaml_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Loading: {yaml_path}")
    with yaml_path.open(encoding="utf-8") as f:
        doc = yaml.safe_load(f)

    text = render_module(doc)
    out_path.write_text(text, encoding="utf-8")
    line_count = text.count("\n") + 1
    print(f"Generated: {out_path} ({line_count} lines)")

    enums = doc.get("enums") or {}
    payloads = doc.get("payloads") or {}
    aux = collect_inline_entry_structs(payloads)
    print()
    print(f"  Enums emitted:           {len(enums)}")
    print(f"  Aux entry-interfaces:    {len(aux)}")
    print(f"  Payload interfaces:      {len(payloads)}")
    print()


if __name__ == "__main__":
    main()
