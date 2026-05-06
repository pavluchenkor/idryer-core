#!/usr/bin/env python3
"""
Validator для mqtt_contract.yaml.

Что делает:
1. Парсит yaml.
2. Валидирует против mqtt_contract.schema.json (структурная проверка).
3. Резолвит cross-references:
   - payload (string в bindings.uart.payload, messages[*].payload) → существует ли в payloads:?
   - enum_ref / ref в FieldSpec → существует ли в enums:?
4. Считает sizeof для каждого payload (грубо, по объявленным типам) — для UART codegen.
5. Проверяет уникальность kind_id в uart_only + bindings.uart.kind_id.
6. Считает суммарную статистику (payloads/messages/...).

Usage:
    python3 validate_contract.py [path/to/mqtt_contract.yaml]

Exit codes:
    0 — валидно
    1 — schema validation errors
    2 — cross-reference errors
    3 — yaml parse error
"""

from __future__ import annotations
import json
import sys
import re
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed. Run: pip3 install pyyaml", file=sys.stderr)
    sys.exit(3)

try:
    import jsonschema
except ImportError:
    print("ERROR: jsonschema not installed. Run: pip3 install jsonschema", file=sys.stderr)
    sys.exit(3)


# ── Sizeof estimator ─────────────────────────────────────────────────────────

PRIMITIVE_SIZES = {
    "uint8":  1, "int8":  1, "bool": 1,
    "uint16": 2, "int16": 2,
    "uint32": 4, "int32": 4, "float": 4,
    "uint64": 8, "int64": 8, "double": 8,
}

ARRAY_RE = re.compile(r"^(?:char|uint8|int8|uint16|int16|uint32|int32|float|double)\[(\d+)\]$")
# Pattern для типов вида "UartTelemetryEntry[4]" — массив структур
NESTED_ARRAY_RE = re.compile(r"^([A-Za-z][A-Za-z0-9_]*)\[(\d+)\]$")


def field_size(field_spec, enums: dict, payloads: dict) -> int | None:
    """Грубая оценка размера поля. None если не удалось определить."""
    if isinstance(field_spec, str):
        return None  # legacy string-form field, skip
    if not isinstance(field_spec, dict):
        return None

    # Skip free-form notes-only entries
    t = field_spec.get("type")
    if t is None:
        return None

    t = str(t)

    # Primitive arrays: uint8[N], char[N], uint16[N], etc.
    m = ARRAY_RE.match(t)
    if m:
        # Извлечь basetype
        basetype = t.split("[")[0]
        per_item = PRIMITIVE_SIZES.get(basetype) or (1 if basetype in ("char", "uint8") else None)
        if per_item is None:
            return None
        return per_item * int(m.group(1))

    # Nested struct array: "UartTelemetryEntry[4]" + item_fields
    m = NESTED_ARRAY_RE.match(t)
    if m:
        struct_name = m.group(1)
        count = int(m.group(2))
        # 1) Если есть inline item_fields — используем их (текущий стиль для большинства массивов)
        item_fields = field_spec.get("item_fields", {})
        if item_fields:
            per_item = sum_fields_size(item_fields, enums, payloads)
            return per_item * count if per_item is not None else None
        # 2) Иначе пробуем резолвить struct_name через payloads — `UartUnitConfig` etc.
        if struct_name in payloads:
            ref_fields = payloads[struct_name].get("fields", {})
            per_item = sum_fields_size(ref_fields, enums, payloads)
            return per_item * count if per_item is not None else None
        return None  # ни inline, ни в payloads — не можем посчитать

    # Primitives
    if t in PRIMITIVE_SIZES:
        size = PRIMITIVE_SIZES[t]
        count = field_spec.get("count")
        if count:
            size *= int(count)
        return size

    # Enum reference
    if t == "enum":
        ref = field_spec.get("ref")
        if ref and ref in enums:
            return 1  # uint8 backing — can be refined per-enum later
        return 1

    # Object with item_fields (array of records, count specified separately)
    if "item_fields" in field_spec:
        item_fields = field_spec["item_fields"]
        count = field_spec.get("count", 1)
        per_item = sum_fields_size(item_fields, enums, payloads)
        return per_item * count if per_item is not None else None

    # String / unknown
    if t == "string":
        # Strings on UART wire are fixed-size char[N] — must use that form. Standalone "string" is unsized.
        return None

    return None


def sum_fields_size(fields: dict, enums: dict, payloads: dict) -> int | None:
    if not isinstance(fields, dict):
        return None
    total = 0
    unknown = []
    for name, spec in fields.items():
        sz = field_size(spec, enums, payloads)
        if sz is None:
            unknown.append(name)
        else:
            total += sz
    if unknown:
        return None  # can't compute total
    return total


# ── Cross-reference resolver ─────────────────────────────────────────────────


def collect_cross_ref_errors(doc: dict) -> list[str]:
    """Резолвит ссылки на payloads и enums, возвращает список ошибок."""
    errors = []
    payloads = set((doc.get("payloads") or {}).keys())
    enums = set((doc.get("enums") or {}).keys())

    # Messages: bindings.uart.payload должны быть в payloads
    for msg in doc.get("messages", []):
        bindings = msg.get("bindings") or {}
        uart = bindings.get("uart") or {}
        pname = uart.get("payload")
        if pname and isinstance(pname, str) and pname not in payloads:
            errors.append(
                f"messages[name={msg.get('name')}].bindings.uart.payload='{pname}' "
                f"— payload не найден в секции payloads:"
            )

    # uart_only: payload (если строка) должен быть в payloads (или 'empty'/'пустой' = empty)
    EMPTY_MARKERS = {"empty", "пустой", "raw bytes (UART_MAX_PAYLOAD)"}
    for u in doc.get("uart_only", []):
        pname = u.get("payload")
        if isinstance(pname, str) and pname not in payloads and not any(
            m in pname for m in EMPTY_MARKERS
        ):
            errors.append(
                f"uart_only[kind={u.get('kind')}].payload='{pname}' "
                f"— payload не найден в секции payloads: и не помечен как 'пустой'/'empty'/'raw bytes'"
            )

    # FieldSpec.ref / enum_ref должны быть в enums (если строка-имя)
    def check_field_refs(fields: dict, ctx: str):
        if not isinstance(fields, dict):
            return
        for fname, fspec in fields.items():
            if not isinstance(fspec, dict):
                continue
            if fspec.get("type") == "enum":
                ref = fspec.get("ref")
                if ref and isinstance(ref, str) and ref not in enums:
                    errors.append(
                        f"{ctx}.{fname}: ref='{ref}' — enum не найден в секции enums:"
                    )
            # nested item_fields
            if "item_fields" in fspec:
                check_field_refs(fspec["item_fields"], f"{ctx}.{fname}.item_fields")

    for pname, pdef in (doc.get("payloads") or {}).items():
        check_field_refs(pdef.get("fields", {}), f"payloads.{pname}.fields")

    return errors


# ── kind_id uniqueness ───────────────────────────────────────────────────────


def normalize_kind_id(value) -> int | None:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 16) if value.lower().startswith("0x") else int(value)
        except ValueError:
            return None
    return None


def check_kind_id_uniqueness(doc: dict) -> list[str]:
    """Проверка уникальности kind_id с учётом cmd_code-дискриминации.

    Логика:
    - Если у entry есть cmd_code — ключ уникальности = (kind_id, cmd_code).
      Это нормально что Command (0x20) встречается много раз с разными cmd_code.
    - Если cmd_code нет — ключ уникальности = просто kind_id.
    """
    errors = []
    seen: dict[tuple, str] = {}

    for u in doc.get("uart_only", []):
        kid = normalize_kind_id(u.get("kind_id"))
        if kid is None:
            continue
        kind = u.get("kind", "<unnamed>")
        cmd_code = u.get("cmd_code")
        cmd_code_id = u.get("cmd_code_id")
        # Ключ: (kind_id, cmd_code_id если есть) — позволяет Command(0x20) с разными cmd_code
        key = (kid, cmd_code_id if cmd_code_id is not None else None)
        label = f"{kind}" + (f" [cmd_code={cmd_code}]" if cmd_code else "")

        if key in seen:
            errors.append(
                f"uart_only: дублирующийся ({hex(kid)}{', cmd_code=' + str(cmd_code_id) if cmd_code_id is not None else ''}) "
                f"у {label} и {seen[key]}"
            )
        else:
            seen[key] = label
    return errors


# ── Main ─────────────────────────────────────────────────────────────────────


def main():
    here = Path(__file__).parent
    yaml_path = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "mqtt_contract.yaml"
    schema_path = here / "mqtt_contract.schema.json"

    if not yaml_path.exists():
        print(f"ERROR: yaml not found: {yaml_path}", file=sys.stderr)
        sys.exit(3)
    if not schema_path.exists():
        print(f"ERROR: schema not found: {schema_path}", file=sys.stderr)
        sys.exit(3)

    print(f"Loading: {yaml_path}")
    print(f"Schema:  {schema_path}")
    print()

    try:
        with yaml_path.open(encoding="utf-8") as f:
            doc = yaml.safe_load(f)
    except yaml.YAMLError as e:
        print(f"YAML parse error:\n{e}", file=sys.stderr)
        sys.exit(3)

    with schema_path.open(encoding="utf-8") as f:
        schema = json.load(f)

    # Schema validation
    print("=" * 70)
    print("STEP 1: Schema validation")
    print("=" * 70)
    validator = jsonschema.Draft202012Validator(schema)
    schema_errors = sorted(validator.iter_errors(doc), key=lambda e: e.path)
    if schema_errors:
        print(f"❌ Found {len(schema_errors)} schema violations:\n")
        for e in schema_errors[:30]:  # cap
            path = "/".join(str(p) for p in e.absolute_path) or "<root>"
            print(f"  • {path}: {e.message[:160]}")
        if len(schema_errors) > 30:
            print(f"  ... and {len(schema_errors) - 30} more")
        print()
    else:
        print("✅ Schema validation passed.\n")

    # Cross-references
    print("=" * 70)
    print("STEP 2: Cross-reference resolution")
    print("=" * 70)
    xref_errors = collect_cross_ref_errors(doc)
    if xref_errors:
        print(f"❌ Found {len(xref_errors)} cross-ref errors:\n")
        for e in xref_errors:
            print(f"  • {e}")
        print()
    else:
        print("✅ All cross-references resolved.\n")

    # kind_id uniqueness
    print("=" * 70)
    print("STEP 3: UART kind_id uniqueness")
    print("=" * 70)
    kid_errors = check_kind_id_uniqueness(doc)
    if kid_errors:
        print(f"❌ Found {len(kid_errors)} kind_id collisions:\n")
        for e in kid_errors:
            print(f"  • {e}")
        print()
    else:
        print("✅ kind_id values unique within uart_only.\n")

    # Sizes
    print("=" * 70)
    print("STEP 4: Payload size computation (best-effort)")
    print("=" * 70)
    enums = doc.get("enums") or {}
    payloads = doc.get("payloads") or {}
    ok_count = 0
    skip_count = 0
    for pname, pdef in payloads.items():
        fields = pdef.get("fields", {})
        size = sum_fields_size(fields, enums, payloads)
        if size is not None:
            print(f"  {pname:30s} = {size:4d} bytes")
            ok_count += 1
        else:
            skip_count += 1
    print(f"\n  Computed: {ok_count}; Skipped (free-form/refs): {skip_count}\n")

    # Stats
    print("=" * 70)
    print("STATS")
    print("=" * 70)
    print(f"  enums:                  {len(doc.get('enums') or {})}")
    print(f"  payloads:               {len(doc.get('payloads') or {})}")
    print(f"  messages:               {len(doc.get('messages') or [])}")
    ia = doc.get("invoke_actions") or {}
    print(f"  invoke_actions.core:    {len(ia.get('core', []))}")
    print(f"  invoke_actions.iheater: {len(ia.get('iheater_link', []))}")
    print(f"  invoke_actions.idryer:  {len(ia.get('idryer_link', []))}")
    print(f"  invoke_actions.rp2040:  {len(ia.get('rp2040', []))}")
    print(f"  uart_only:              {len(doc.get('uart_only') or [])}")
    print(f"  mqtt_only:              {len(doc.get('mqtt_only') or [])}")
    print(f"  legacy_command_topics:  {len(doc.get('legacy_command_topics') or [])}")
    print(f"  known_mismatches:       {len(doc.get('known_mismatches') or [])}")
    print()

    # Final verdict
    print("=" * 70)
    if schema_errors:
        print("VERDICT: ❌ INVALID — schema violations")
        sys.exit(1)
    if xref_errors or kid_errors:
        print("VERDICT: ❌ INVALID — cross-reference or kind_id errors")
        sys.exit(2)
    print("VERDICT: ✅ VALID — ready for codegen pipeline")
    sys.exit(0)


if __name__ == "__main__":
    main()
