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
import json
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


# ── HardwareUnitConfigCapabilities ─────────────────────────────────


def render_capabilities_interface(doc: dict) -> str:
    """
    Generates HardwareUnitConfigCapabilities from capability_vocabulary.
    This is the TypeScript counterpart of iDryer::Config has* flags.
    Keys match units[].capabilities JSON keys published by firmware in /info.
    """
    vocab = doc.get("capability_vocabulary") or {}
    out = []
    out.append("/**")
    out.append(" * Device peripheral capabilities reported by firmware in /info → units[].capabilities.")
    out.append(" * Generated from capability_vocabulary in mqtt_contract.yaml.")
    out.append(" * DO NOT EDIT — run contracts/regen.sh to regenerate.")
    out.append(" */")
    out.append("export interface HardwareUnitConfigCapabilities {")
    for cap_name, cap in vocab.items():
        desc = cap.get("description", "")
        out.append(f"  /** {desc} */")
        out.append(f"  {cap_name}?: boolean;")
    out.append("}")
    return "\n".join(out)


# ── CanonicalRoles ─────────────────────────────────────────────────


def render_canonical_roles(doc: dict) -> str:
    roles = doc.get("canonical_roles") or {}
    out = []
    out.append("/**")
    out.append(" * Canonical roles from mqtt_contract.yaml → canonical_roles.")
    out.append(" * Portal uses these to identify known menu items and pick widgets.")
    out.append(" * DO NOT EDIT — run contracts/regen.sh to regenerate.")
    out.append(" */")
    out.append("export const CanonicalRoles = {")
    for role_name, role_def in roles.items():
        if not isinstance(role_def, dict):
            continue
        rtype  = role_def.get("type", "unknown")
        widget = role_def.get("widget", "button" if rtype == "action" else "number")
        unit   = role_def.get("unit", "")
        labels = role_def.get("labels") or {}
        labels_str = ", ".join(f'"{k}": "{v}"' for k, v in labels.items())
        labels_ts  = f"{{ {labels_str} }}" if labels_str else "{}"
        out.append(f'  "{role_name}": {{ type: "{rtype}", widget: "{widget}", unit: "{unit}", labels: {labels_ts} }},')
    out.append("} as const;")
    out.append("")
    out.append("export type CanonicalRole = keyof typeof CanonicalRoles;")
    out.append("export type WidgetName = typeof CanonicalRoles[CanonicalRole][\"widget\"];")
    return "\n".join(out)


# ── Role i18n JSON files ───────────────────────────────────────────


def generate_roles_i18n_files(doc: dict, out_dir: Path) -> list[str]:
    """Generate roles.{lang}.json for each language found in canonical_roles.labels.
    Each file is a flat dict: { "role.name": "Translation" }.
    Missing translations fall back to "en". Returns list of written file paths.
    """
    roles = doc.get("canonical_roles") or {}

    langs: set[str] = set()
    for role_def in roles.values():
        if isinstance(role_def, dict):
            langs.update((role_def.get("labels") or {}).keys())

    written = []
    for lang in sorted(langs):
        data: dict[str, str] = {}
        for role_name, role_def in roles.items():
            if not isinstance(role_def, dict):
                continue
            labels = role_def.get("labels") or {}
            data[role_name] = labels.get(lang) or labels.get("en") or role_name
        out_file = out_dir / f"roles.{lang}.json"
        out_file.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        written.append(str(out_file))
    return written


# ── InvokeActions ──────────────────────────────────────────────────


def render_invoke_actions(doc: dict) -> str:
    """
    Generates InvokeActions const from invoke_actions section.
    For each action, emits enum arg values and string encodings
    so the frontend can build selectors without hardcoding.
    """
    invoke_actions = doc.get("invoke_actions") or {}
    out = []
    out.append("/**")
    out.append(" * Invoke action arg schemas from mqtt_contract.yaml.")
    out.append(" * Enum values and string encodings for UI selector construction.")
    out.append(" * DO NOT EDIT — run contracts/regen.sh to regenerate.")
    out.append(" */")
    out.append("export const InvokeActions = {")

    for device_name, actions in invoke_actions.items():
        if not isinstance(actions, list) or not actions:
            continue
        out.append(f'  "{device_name}": {{')
        for action in actions:
            action_name = action.get("name")
            if not action_name:
                continue
            args = action.get("args") or {}
            if action_name.startswith("<"):
                continue
            arg_lines = []
            for arg_name, arg_spec in args.items():
                if not isinstance(arg_spec, dict):
                    continue
                if arg_spec.get("type") == "enum":
                    values = arg_spec.get("values", [])
                    values_str = ", ".join(f'"{v}"' for v in values)
                    arg_lines.append(f'      {arg_name}: [{values_str}] as const,')
                elif arg_spec.get("encoding"):
                    encoding = arg_spec["encoding"]
                    arg_lines.append(f'      {arg_name}Encoding: "{encoding}",')
            if arg_lines:
                out.append(f'    "{action_name}": {{')
                out.extend(arg_lines)
                out.append(f'    }},')
            else:
                out.append(f'    "{action_name}": {{}},')
        out.append(f'  }},')

    out.append("} as const;")
    return "\n".join(out)


# ── DeviceProfiles ──────────────────────────────────────────────────


def render_device_profiles(doc: dict) -> str:
    profiles = doc.get("device_profiles") or {}
    vocab = doc.get("capability_vocabulary") or {}
    all_caps = list(vocab.keys())

    out = []
    out.append("/**")
    out.append(" * Known device profiles — which capabilities each product type has.")
    out.append(" * Generated from device_profiles in mqtt_contract.yaml.")
    out.append(" */")
    out.append("export const DeviceCapabilityProfiles: Record<string, HardwareUnitConfigCapabilities> = {")
    for profile_name, profile in profiles.items():
        caps = profile.get("capabilities") or []
        obj_entries = ", ".join(
            f"{c}: true" for c in all_caps if c in caps
        )
        out.append(f'  "{profile_name}": {{ {obj_entries} }},')
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

    # Canonical roles
    out.append("// ── Canonical roles (from canonical_roles) ────────────────────────")
    out.append("")
    out.append(render_canonical_roles(doc))
    out.append("")

    # Invoke action schemas
    out.append("// ── Invoke action schemas (from invoke_actions) ───────────────────")
    out.append("")
    out.append(render_invoke_actions(doc))
    out.append("")

    # Capabilities
    out.append("// ── Device capabilities (from capability_vocabulary) ──────────────")
    out.append("")
    out.append(render_capabilities_interface(doc))
    out.append("")
    out.append(render_device_profiles(doc))
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

    i18n_files = generate_roles_i18n_files(doc, out_dir)
    for f in i18n_files:
        print(f"Generated: {f}")
    if i18n_files:
        print()


if __name__ == "__main__":
    main()
