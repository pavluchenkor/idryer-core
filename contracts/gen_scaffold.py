#!/usr/bin/env python3
"""
gen_scaffold.py — генератор PlatformIO-проекта из device_profiles в mqtt_contract.yaml.

Использование:
    python3 gen_scaffold.py                  # все профили
    python3 gen_scaffold.py storage_link     # один профиль

Что генерирует для каждого профиля:
    contracts/_generated/scaffolds/{profile}/
        src/main.cpp          — устройство на iDryer::Link: Config из профиля,
                                действия карточки по режимам юнита, TODO
        platformio.ini        — окружения prod и stage
        README.md             — как запустить

Wi-Fi и привязку ядро поднимает само (приложение iDryer, Improv), поэтому
ни паролей сети, ни secrets.h в заготовке нет.

Источник правды:
    mqtt_contract.yaml → device_profiles + capability_vocabulary + unit_modes_per_product
"""

from __future__ import annotations
import sys
import datetime
import textwrap
from pathlib import Path
import yaml

HERE = Path(__file__).parent

# Режим юнита на проводе → UnitMode фасада.
UNIT_MODE_ENUM = {
    "IDLE": "Idle",
    "DRYING": "Drying",
    "STORAGE": "Storage",
    "PROFILE": "Profile",
    "HEATING": "Heating",
    "LIGHT_ANIMATION": "LightAnimation",
}

# Параметры действия запуска для режима: подсказка, пределы — TODO продукта.
START_PARAMS = {
    "DRYING": [
        '.param("temperature", "target_temperature", 30, 90, 1, 50, "°C")',
        '.param("duration", "duration", 0, 2880, 10, 240, "min")',
    ],
    "HEATING": [
        '.param("temperature", "target_temperature", 30, 70, 1, 45, "°C")',
        '.param("duration", "duration", 0, 720, 10, 120, "min")',
    ],
    "LIGHT_ANIMATION": [
        '.select("effect", "effect", kEffects, 2, "solid")',
        '.color("color", "rgb_color", "#FFFFFF")',
    ],
}


# ── Helpers ──────────────────────────────────────────────────────────────────

def _device_type_enum(profile: dict) -> str:
    """device_profiles.*.device_type → iDryer::DeviceType."""
    mapping = {
        "storage_link": "StorageLink",
        "iheater_link": "IHeaterLink",
        "dryer": "Dryer",
    }
    return mapping.get(profile.get("device_type", ""), "Unknown")


def _config_flag(cap: str, entry: dict) -> str:
    """air_temp → hasAirTemp (или config_flag из словаря)."""
    return entry.get("config_flag") or "has" + "".join(p.capitalize() for p in cap.split("_"))


def _start_mode(profile_name: str, doc: dict) -> str | None:
    modes = ((doc.get("unit_modes_per_product") or {}).get(profile_name) or {}).get("active") or []
    return modes[0] if modes else None


# ── Generators ───────────────────────────────────────────────────────────────

def gen_main_cpp(profile_name: str, profile: dict, doc: dict) -> str:
    vocab = doc.get("capability_vocabulary") or {}
    capabilities: list[str] = profile.get("capabilities") or []
    today = datetime.date.today().isoformat()
    caps_str = ", ".join(capabilities) if capabilities else "none"
    modes = ((doc.get("unit_modes_per_product") or {}).get(profile_name) or {}).get("active") or []
    start_mode = _start_mode(profile_name, doc)

    L: list[str] = []
    def w(line: str = "") -> None:
        L.append(line)

    w("// ============================================================================")
    w(f"// SCAFFOLD: {profile_name}")
    w(f"// Generated {today} by contracts/gen_scaffold.py from mqtt_contract.yaml")
    w("//")
    w("// HOW TO START:")
    w("//   1. Copy this directory; put idryer-core into lib/idryer-core")
    w("//      (a copy, a git submodule or a symbolic link).")
    w("//   2. Fill in the TODO sections with your hardware logic.")
    w(f"//   3. pio run -e {profile_name}-prod -t upload")
    w("//   4. Wi-Fi and pairing: the iDryer app, \"Connect a new device\".")
    w("//")
    w(f"// Capabilities: {caps_str}")
    w(f"// Unit modes:   {', '.join(modes) if modes else 'none'}")
    w("// ============================================================================")
    w()
    w("#include <Arduino.h>")
    w("#include <iDryer.h>")
    w()
    w(f"// Flags from device_profiles.{profile_name} in mqtt_contract.yaml.")
    w("static const iDryer::Config CFG = {")
    w(f"    .deviceType      = iDryer::DeviceType::{_device_type_enum(profile)},")
    w("    .unitsCount      = 1,   // TODO: number of units (chambers)")
    # Порядок полей — как в Config (словарь идёт в порядке объявления флагов).
    for cap, entry in vocab.items():
        if cap in capabilities:
            flag = _config_flag(cap, entry or {})
            desc = (entry or {}).get("description", cap)
            w(f"    .{flag:<15} = true,   // {desc}")
    if profile.get("ota_interrupt"):
        w(f"    .otaInterrupt    = iDryer::OTA_INTERRUPT_{profile_name.upper()},")
    w('    .hardwareVersion = "1.0",')
    w('    .firmwareVersion = "0.1.0",')
    w(f'    .model           = "{profile_name}",')
    w("};")
    w("static iDryer::Link s_link(CFG);")
    w()
    if start_mode == "LIGHT_ANIMATION":
        w('static const char* const kEffects[] = { "solid", "breathe" };')
        w()
    if start_mode:
        w("// Card action: start. Numbers in args are already clamped to the limits.")
        w("static void onStart(uint8_t unit, JsonObjectConst args) {")
        w("    // TODO: start the hardware with args.")
        w("    (void)args;")
        w(f"    s_link.status.mode[unit] = iDryer::UnitMode::{UNIT_MODE_ENUM.get(start_mode, 'Unknown')};")
        w("    s_link.publishStatusNow();")
        w("}")
        w()
        w("// Card action: stop.")
        w("static void onStop(uint8_t unit, JsonObjectConst) {")
        w("    // TODO: stop the hardware.")
        w("    s_link.status.mode[unit] = iDryer::UnitMode::Idle;")
        w("    s_link.publishStatusNow();")
        w("}")
        w()
    w("void setup() {")
    w("    s_link.begin();")
    w("    // Unlinking in the app or on the portal: erase the secret, wait for pairing.")
    w('    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });')
    if start_mode:
        w()
        w("    // Card actions: the mode after the action and its start parameters.")
        w("    // TODO: limits of your device.")
        w("    auto& card = s_link.card();")
        params = START_PARAMS.get(start_mode, [])
        w(f'    card.action("start", "{start_mode}", onStart)' + ("" if params else ";"))
        for i, p in enumerate(params):
            w(f"        {p}" + (";" if i == len(params) - 1 else ""))
        w('    card.action("stop", "IDLE", onStop);')
    w("    // More on the card: docs/*/09-add-product/02-add-widget.md")
    w("}")
    w()
    w("void loop() {")
    w("    s_link.loop();")
    tele = [(cap, (vocab.get(cap) or {}).get("telemetry_field")) for cap in capabilities]
    tele = [(cap, f) for cap, f in tele if f]
    if tele:
        w()
        w("    // TODO: read the hardware; the core publishes telemetry itself.")
        w("    // NAN in a float field means no data: the field is not sent.")
        for cap, field in tele:
            w(f"    // s_link.telemetry.{field}[0] = ...;   // {cap}")
    w("}")
    return "\n".join(L) + "\n"


def gen_platformio_ini(profile_name: str) -> str:
    return textwrap.dedent(f"""\
        ; PlatformIO config for {profile_name}
        ; Generated by contracts/gen_scaffold.py
        ;
        ; The core lives in lib/idryer-core; its library.json brings MQTT,
        ; ArduinoJson, WebSockets and Improv. lib_deps: your sensor libraries only.

        [platformio]
        default_envs = {profile_name}-prod

        [env]
        platform      = espressif32
        board         = esp32-c3-devkitm-1
        framework     = arduino
        monitor_speed = 115200
        ; ESPAsyncTCP is the ESP8266 transport from espMqttClient dependencies:
        ; it does not build on ESP32.
        lib_ignore = ESPAsyncTCP

        ; Board without USB-UART (ESP32-C3 SuperMini): Serial over USB.
        [flags_usb_cdc]
        build_flags =
          -DARDUINO_USB_MODE=1
          -DARDUINO_USB_CDC_ON_BOOT=1

        [flags_prod]
        build_flags =
          -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
          -DMQTT_BROKER='"mqtt.idryer.org"'
          -DMQTT_PORT=8883
          -DMQTT_USE_TLS=1

        [flags_stage]
        build_flags =
          -DIDRYER_API_BASE='"https://staging.idryer.org/api"'
          -DMQTT_BROKER='"staging.idryer.org"'
          -DMQTT_PORT=1884
          -DMQTT_USE_TLS=0

        [env:{profile_name}-prod]
        build_flags =
          ${{flags_usb_cdc.build_flags}}
          ${{flags_prod.build_flags}}

        [env:{profile_name}-stage]
        build_flags =
          ${{flags_usb_cdc.build_flags}}
          ${{flags_stage.build_flags}}
        """)


def gen_readme(profile_name: str, profile: dict) -> str:
    capabilities: list[str] = profile.get("capabilities") or []
    caps_str = ", ".join(capabilities) if capabilities else "none"
    return textwrap.dedent(f"""\
        # {profile_name}

        Auto-generated scaffold on `iDryer::Link`. Capabilities: **{caps_str}**.

        ## Quick start

        1. Copy this directory and put idryer-core into `lib/idryer-core`
           (a copy, a git submodule or a symbolic link).
        2. Fill the `TODO` sections in `src/main.cpp` with your hardware logic.
        3. Build and flash: `pio run -e {profile_name}-prod -t upload`.
        4. Wi-Fi and pairing: in the iDryer app tap **Connect a new device**,
           pass the network on the **Wi-Fi** step and pair on the **Pairing** step.

        The firmware has no network password and no account data: the core
        gets both from the app.

        ## Next

        - Quick start: `docs/en/02-quickstart/`.
        - Device card, actions and parameters: `docs/en/09-add-product/02-add-widget.md`.
        """)


# ── Main ─────────────────────────────────────────────────────────────────────

def generate_profile(profile_name: str, profile: dict, doc: dict, out_root: Path) -> None:
    capabilities: list[str] = profile.get("capabilities") or []

    out_dir = out_root / profile_name
    (out_dir / "src").mkdir(parents=True, exist_ok=True)

    files = {
        out_dir / "src" / "main.cpp": gen_main_cpp(profile_name, profile, doc),
        out_dir / "platformio.ini":   gen_platformio_ini(profile_name),
        out_dir / "README.md":        gen_readme(profile_name, profile),
    }

    for path, content in files.items():
        path.write_text(content, encoding="utf-8")

    print(f"  {profile_name}/  ({', '.join(capabilities) or 'no capabilities'})")
    for path in files:
        rel = path.relative_to(out_root.parent)
        print(f"    → {rel}")


def main() -> None:
    yaml_path = HERE / "mqtt_contract.yaml"
    out_root  = HERE / "_generated" / "scaffolds"

    if not yaml_path.exists():
        print(f"ERROR: {yaml_path} not found", file=sys.stderr)
        sys.exit(1)

    with yaml_path.open(encoding="utf-8") as f:
        doc = yaml.safe_load(f)

    profiles: dict = doc.get("device_profiles") or {}
    if not profiles:
        print("No device_profiles found in contract.", file=sys.stderr)
        sys.exit(1)

    # Filter by CLI argument if provided.
    target = sys.argv[1] if len(sys.argv) > 1 else None
    if target and target not in profiles:
        print(f"ERROR: profile '{target}' not found. Available: {list(profiles)}", file=sys.stderr)
        sys.exit(1)

    selected = {target: profiles[target]} if target else profiles

    print(f"Generating scaffolds → {out_root}/")
    for name, profile in selected.items():
        generate_profile(name, profile, doc, out_root)

    print(f"\n✅ {len(selected)} scaffold(s) generated.")
    print("   Copy the folder you need to your PlatformIO project,")
    print("   put idryer-core into lib/idryer-core.")


if __name__ == "__main__":
    main()
