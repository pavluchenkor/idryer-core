#!/usr/bin/env python3
"""
gen_idryer_api_h.py — генератор `_generated/iDryer_api.h` из mqtt_contract.yaml.

Что генерирует:
  1. Enums данных (UnitMode/DeviceType/IntegrationKind/IntegrationState/EventKind/RequestKind).
  2. Data-structs (Telemetry/Status/Request/ProfileStage/ProfileSchedule/IntegrationStatus).
  3. Config — с per-capability флагами и runtime-полями.

Что НЕ генерирует (остаётся в ручном `iDryer.h`):
  - class Link — это API SDK, не данные.
  - реализация фасада (iDryer.cpp).

Output:
  contracts/_generated/iDryer_api.h

Источник правды — mqtt_contract.yaml + ручные mapping'и для facade-specific
концепций (Request/Config), которых в yaml нет напрямую.
"""

from __future__ import annotations
import sys
import datetime
from pathlib import Path
import yaml


# ── Mapping helpers ─────────────────────────────────────────────────────────


# yaml UartDryerMode → facade UnitMode (1-в-1 + Unknown как fallback).
UNIT_MODE_FROM_DRYER_MODE = [
    ("Idle",    "unit not running"),
    ("Drying",  "active drying session"),
    ("Storage", "storage mode (gentle low-power keep-dry)"),
    ("Profile", "running multi-step profile"),
    ("Fault",   "unrecoverable hardware error"),
    ("Unknown", "fallback for unrecognized UART mode (does not close session)"),
]

# RequestKind = подмножество commands направления backend_to_device, которые
# фасад пробрасывает в onRequest. profile/read_rfid/write_rfid намеренно
# исключены (см. iDryer.h комментарий).
REQUEST_KIND_FROM_COMMAND = {
    "drying":       ("Start",       "commands/drying — begin drying session"),
    "stop":         ("Stop",        "commands/stop   — stop running session"),
    "storage":      ("Storage",     "commands/storage — switch unit to storage mode"),
    "find":         ("Find",        "commands/find   — indicate physical location"),
    "clear_errors": ("ClearErrors", "commands/clear_errors"),
}

# Telemetry-поля: yaml-имя → (тип C, имя поля Config.has*, имя поля Telemetry).
# Источник правды — то что фасад реально публикует (см. iDryer.cpp publishTelemetryNow).
TELEMETRY_FIELDS = [
    # (json_key,         c_type,    array,   has_flag,         doc)
    ("temperature",      "float",   True,    "hasAirTemp",     "air temperature, °C"),
    ("humidity",         "float",   True,    "hasAirHumidity", "air humidity, %"),
    ("heaterTemp",       "float",   True,    "hasHeaterTemp",  "heater body temperature, °C"),
    ("heaterPower",      "float",   True,    "hasHeaterPower", "normalized heater power, 0..1"),
    ("fanStatus",        "bool",    True,    "hasFanStatus",   "fan running"),
    ("weight",           "uint16_t","True",  "hasScales",      "filament weight, grams"),
]

# Telemetry struct field names that user fills (different from json_keys).
TELEMETRY_USER_FIELDS = [
    ("airTempC",         "float",     "hasAirTemp"),
    ("airHumidityPct",   "float",     "hasAirHumidity"),
    ("heaterTempC",      "float",     "hasHeaterTemp"),
    ("heaterPower01",    "float",     "hasHeaterPower"),
    ("fanOn",            "bool",      "hasFanStatus"),
    ("weightG",          "uint16_t",  "hasScales"),
]

# Integrations — флаги Config.allow* (compile-time выбор).
INTEGRATIONS = [
    ("Ha",        "allowHa",        "Home Assistant MQTT integration"),
    ("Bambu",     "allowBambu",     "Bambu Lab printer LAN MQTT"),
    ("Moonraker", "allowMoonraker", "Moonraker WebSocket JSON-RPC"),
]


# ── Render helpers ──────────────────────────────────────────────────────────


def render_unit_mode_enum(_doc: dict) -> list[str]:
    """UnitMode — устройство-режим. Yaml: enums.UartDryerMode (+ PortalUnitStatus.UNKNOWN)."""
    out = [
        "/// What the unit is currently doing.",
        "/// Mirrors yaml.enums.UartDryerMode + PortalUnitStatus.UNKNOWN fallback.",
        "enum class UnitMode : uint8_t {",
    ]
    for name, doc in UNIT_MODE_FROM_DRYER_MODE:
        out.append(f"    {name},   ///< {doc}")
    out.append("};")
    return out


def render_device_type_enum(doc: dict) -> list[str]:
    """DeviceType — facade-level product types. Subset/rename of yaml.enums.UartDeviceType.

    Wire-уровень (UART/info.deviceType) у нас более общий: 'Link' = любое ESP-only
    устройство без UART-MCU. На facade-уровне продукт идентифицируется конкретно
    (StorageLink, ...), поэтому здесь делаем mapping yaml-name → facade-name.

    Telemetry — wire-only тип (UART-нода-телеметрист), в фасадном enum не нужен.
    """
    # yaml-name → facade-name. Включай новые продукты добавлением сюда строки.
    PRODUCT_RENAME = {
        "Unknown":     "Unknown",
        "Dryer":       "Dryer",
        "Heater":      "Heater",
        "Link":        "StorageLink",   # wire 'Link' (0x04) → facade 'StorageLink'
        "IHeaterLink": "IHeaterLink",
    }

    enums = (doc.get("enums") or {})
    udt = enums.get("UartDeviceType")
    if not udt:
        return ["// (UartDeviceType not found in yaml)"]
    items = list(udt.items()) if isinstance(udt, dict) else [(v, i) for i, v in enumerate(udt)]
    items = [(PRODUCT_RENAME[n], v) for n, v in items if n in PRODUCT_RENAME]
    items.sort(key=lambda kv: kv[1] if isinstance(kv[1], int) else 0)
    out = [
        "/// Product device family — set in Config and reflected in info.deviceType.",
        "/// Mapping yaml.enums.UartDeviceType → facade DeviceType (см. gen_idryer_api_h.py).",
        "/// 'Link' (0x04) на проводе → StorageLink в фасаде (продуктовое имя).",
        "enum class DeviceType : uint8_t {",
    ]
    for name, val in items:
        out.append(f"    {name} = {val},")
    out.append("};")
    return out


def render_integration_kind_enum() -> list[str]:
    out = [
        "/// Integration channel — used for IntegrationStatus and Config.allow* flags.",
        "enum class IntegrationKind : uint8_t {",
    ]
    for name, _flag, doc in INTEGRATIONS:
        out.append(f"    {name},   ///< {doc}")
    out.append("};")
    return out


def render_integration_state_enum(doc: dict) -> list[str]:
    """IntegrationState — из yaml.enums.IntegrationState.cpp_enum.values."""
    enums = doc.get("enums") or {}
    spec = enums.get("IntegrationState") or {}
    cpp_values = ((spec.get("cpp_enum") or {}).get("values") or [])
    if not cpp_values:
        cpp_values = ["Inactive", "Connecting", "Active", "Error"]
    out = [
        "/// Integration connectivity health (observation, not control).",
        "/// Mirrors yaml.enums.IntegrationState.cpp_enum.",
        "enum class IntegrationState : uint8_t {",
    ]
    if isinstance(cpp_values, dict):
        items = sorted(cpp_values.items(), key=lambda kv: kv[1])
        for name, val in items:
            out.append(f"    {name} = {val},")
    else:
        for i, name in enumerate(cpp_values):
            out.append(f"    {name} = {i},")
    out.append("};")
    return out


def render_event_kind_enum() -> list[str]:
    out = [
        "/// Event severity for raiseEvent() — JSON `severity` per contract.",
        "/// Mirrors yaml.enums.PortalEventType severities (INFO/WARNING/ERROR).",
        "enum class EventKind : uint8_t {",
        "    Info,",
        "    Warning,",
        "    Error,",
        "};",
    ]
    return out


def render_request_kind_enum(_doc: dict) -> list[str]:
    out = [
        "/// Business commands routed through Link::onRequest().",
        "/// Generated from yaml messages with direction=backend_to_device.",
        "/// commands/profile uses Link::onProfile() (separate); commands/read_rfid /",
        "/// write_rfid have RPC reply path and are not yet exposed via the facade.",
        "enum class RequestKind : uint8_t {",
    ]
    for cmd_name, (kind, doc) in REQUEST_KIND_FROM_COMMAND.items():
        out.append(f"    {kind},   ///< {doc}")
    out.append("};")
    return out


def render_telemetry_struct() -> list[str]:
    out = [
        "/// User-filled outgoing telemetry. Published every Config.telemetryPeriodMs.",
        "/// Fields whose Config.has* flag is false are omitted from JSON.",
        "struct Telemetry {",
    ]
    for name, ctype, _ in TELEMETRY_USER_FIELDS:
        out.append(f"    {ctype:<10} {name}[MAX_UNITS];")
    out.append("};")
    return out


def render_status_struct() -> list[str]:
    out = [
        "/// User-filled operational status. Published every Config.statusPeriodMs",
        "/// and immediately on Link::publishStatusNow() (e.g. after a command).",
        "struct Status {",
        "    UnitMode mode[MAX_UNITS];",
        "    float    targetTempC[MAX_UNITS];",
        "    uint32_t durationS[MAX_UNITS];   ///< requested duration in seconds; 0 = infinite",
        "    uint32_t elapsedS[MAX_UNITS];    ///< since session started",
        "};",
    ]
    return out


def render_request_struct() -> list[str]:
    out = [
        "/// Incoming business command — dispatched via Link::onRequest().",
        "struct Request {",
        "    RequestKind kind;",
        "    uint8_t     unitId;          ///< 0..unitsCount-1, or 0xFF for broadcast",
        "    float       targetTempC;     ///< Start, Storage. 0 if portal omitted.",
        "    uint32_t    durationS;       ///< Start. 0 = infinite OR not provided.",
        "};",
    ]
    return out


def render_profile_structs() -> list[str]:
    out = [
        "/// One stage of a multi-stage profile (yaml.payloads.Profile.stages item).",
        "struct ProfileStage {",
        "    float    tempC;",
        "    uint32_t rampS;",
        "    uint32_t holdS;",
        "};",
        "",
        "/// commands/profile payload — routed via Link::onProfile().",
        "struct ProfileSchedule {",
        "    static constexpr uint8_t MAX_STAGES = 10;",
        "    uint8_t      unitId;",
        "    uint8_t      startStage;",
        "    uint8_t      stageCount;",
        "    ProfileStage stages[MAX_STAGES];",
        "};",
    ]
    return out


def render_integration_status_struct() -> list[str]:
    return [
        "/// Integration health snapshot — observation only.",
        "struct IntegrationStatus {",
        "    IntegrationKind  kind;",
        "    IntegrationState state;",
        "};",
    ]


def render_config_struct() -> list[str]:
    out = [
        "/// Capability declaration — what this build of the device supports.",
        "/// Filled once by product main.cpp, passed to Link constructor.",
        "/// has* flags are 1-to-1 with Telemetry fields; allow* flags gate integrations.",
        "struct Config {",
        "    DeviceType  deviceType;",
        "    uint8_t     unitsCount;          ///< 1..MAX_UNITS",
        "",
        "    // ── Telemetry capabilities (1:1 with struct Telemetry fields) ──",
    ]
    for name, _ctype, flag in TELEMETRY_USER_FIELDS:
        out.append(f"    bool        {flag};")
    out += [
        "",
        "    // ── Other product capabilities (not directly in Telemetry) ──",
        "    bool        hasRfid;             ///< RFID reader (units[].rfid array, rfid topic)",
        "",
        "    // ── Integration availability (compile-time decision) ──",
    ]
    for _kind, flag, _doc in INTEGRATIONS:
        out.append(f"    bool        {flag};")
    out += [
        "",
        "    // ── Auto-publish periods (ms) ──",
        "    uint32_t    telemetryPeriodMs;",
        "    uint32_t    statusPeriodMs;",
        "",
        "    // ── Identification (published in `info` retained payload) ──",
        "    const char* hardwareVersion;",
        "    const char* firmwareVersion;",
        "};",
    ]
    return out


# ── Main ────────────────────────────────────────────────────────────────────


def render_module(doc: dict) -> str:
    today = datetime.date.today().isoformat()
    yaml_version = doc.get("version", "?")

    out = [
        "// ============================================================================",
        "// AUTO-GENERATED by contracts/gen_idryer_api_h.py from mqtt_contract.yaml",
        "// DO NOT EDIT — изменения вносятся в yaml + manual mappings, потом регенерация.",
        "//",
        f"// Generated: {today}",
        f"// Yaml version: {yaml_version}",
        "// ============================================================================",
        "",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "namespace iDryer {",
        "",
        "/// Maximum number of units (chambers). Fixed by protocol.",
        "constexpr uint8_t MAX_UNITS = 4;",
        "",
        "// ── Enums ─────────────────────────────────────────────────────────",
        "",
    ]
    out += render_unit_mode_enum(doc) + [""]
    out += render_device_type_enum(doc) + [""]
    out += render_integration_kind_enum() + [""]
    out += render_integration_state_enum(doc) + [""]
    out += render_event_kind_enum() + [""]
    out += render_request_kind_enum(doc) + [""]

    out += [
        "// ── Data structs ──────────────────────────────────────────────────",
        "",
    ]
    out += render_telemetry_struct() + [""]
    out += render_status_struct() + [""]
    out += render_request_struct() + [""]
    out += render_profile_structs() + [""]
    out += render_integration_status_struct() + [""]

    out += [
        "// ── Config ────────────────────────────────────────────────────────",
        "",
    ]
    out += render_config_struct() + [""]

    out += [
        "} // namespace iDryer",
        "",
    ]
    return "\n".join(out)


def main():
    here = Path(__file__).parent
    yaml_path = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "mqtt_contract.yaml"
    # Output goes into lib/idryer-core/src/_generated/ so PlatformIO library
    # auto-discovery picks it up without extra include paths.
    out_dir = here.parent / "src" / "_generated"
    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / "iDryer_api.h"

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
    print()


if __name__ == "__main__":
    main()
