#!/usr/bin/env python3
"""
gen_scaffold.py — генератор PlatformIO-проекта из device_profiles в mqtt_contract.yaml.

Использование:
    python3 gen_scaffold.py                  # все профили
    python3 gen_scaffold.py storage_link     # один профиль

Что генерирует для каждого профиля:
    contracts/_generated/scaffolds/{profile}/
        src/main.cpp          — composition root с заполненным Config и TODO-комментариями
        include/secrets.h     — шаблон credentials
        platformio.ini        — минимальный рабочий env
        README.md             — 5 шагов «как запустить»

Источник правды:
    mqtt_contract.yaml → device_profiles + capability_vocabulary + invoke_actions
"""

from __future__ import annotations
import sys
import datetime
import textwrap
from pathlib import Path
import yaml

HERE = Path(__file__).parent


# ── Helpers ──────────────────────────────────────────────────────────────────

def _to_class_name(profile_name: str) -> str:
    """storage_link → StorageLink"""
    return "".join(w.capitalize() for w in profile_name.split("_"))


def _device_type_enum(profile_name: str) -> str:
    """Map profile name to idryer::DeviceType enum value."""
    mapping = {
        "storage_link": "StorageLink",
        "iheater_link": "IHeaterLink",
        "dryer_v3":     "Dryer",
    }
    return mapping.get(profile_name, "Unknown")


def _collect_invoke_hints(profile_name: str, doc: dict) -> list[str]:
    """Return lines of code comments listing available invoke actions."""
    invoke_actions = doc.get("invoke_actions") or {}
    product_actions = invoke_actions.get(profile_name) or []
    if not product_actions:
        return []
    lines = ["    // Actions available for this profile (from invoke_actions in mqtt_contract.yaml):"]
    for act in product_actions:
        name = act.get("name", "?")
        purpose = act.get("purpose", "").strip().split("\n")[0][:80]
        args = act.get("args") or {}
        arg_names = ", ".join(args.keys()) if args else "—"
        lines.append(f"    //   {name}  (args: {arg_names})  — {purpose}")
    lines.append("    // Example dispatch:")
    if product_actions:
        first = product_actions[0].get("name", "my.action")
        lines.append(f'    // if (strcmp(action, "{first}") == 0) {{ /* TODO */ return true; }}')
    return lines


def _capabilities_json_lines(capabilities: list[str], vocab: dict) -> list[str]:
    """Generate doc["capabilities"]["led"] = true; lines for buildInfoJson."""
    if not capabilities:
        return []
    lines = ["        JsonObject caps = doc.createNestedObject(\"capabilities\");"]
    for cap in capabilities:
        entry = vocab.get(cap) or {}
        json_key = entry.get("json_key", cap)
        lines.append(f'        caps["{json_key}"] = true;')
    return lines


# ── Generators ───────────────────────────────────────────────────────────────

def gen_main_cpp(profile_name: str, capabilities: list[str], doc: dict) -> str:
    vocab = doc.get("capability_vocabulary") or {}
    class_name = _to_class_name(profile_name)
    device_type = _device_type_enum(profile_name)
    today = datetime.date.today().isoformat()
    caps_str = ", ".join(capabilities) if capabilities else "none"

    # ── Config capability flags ──────────────────────────────────────────────
    cap_flag_lines: list[str] = []
    for cap in capabilities:
        entry = vocab.get(cap) or {}
        flag  = entry.get("config_flag", f"has{cap.capitalize()}")
        desc  = entry.get("description", cap)
        cap_flag_lines.append(f"    .{flag:<22} = true,   // {desc}")
    for cap, entry in vocab.items():
        if cap not in capabilities:
            flag = entry.get("config_flag", f"has{cap.capitalize()}")
            cap_flag_lines.append(f"    .{flag:<22} = false,  // (not in this profile)")

    # ── buildInfoJson capabilities ───────────────────────────────────────────
    caps_json_lines = _capabilities_json_lines(capabilities, vocab)
    if not caps_json_lines:
        caps_json_lines = ["        // No capabilities — add them here."]

    # ── invoke hints ─────────────────────────────────────────────────────────
    invoke_hint_lines = _collect_invoke_hints(profile_name, doc)
    if not invoke_hint_lines:
        invoke_hint_lines = ["    // No invoke actions defined for this profile yet."]

    # Build output line-by-line to avoid dedent/indentation issues.
    L = []
    def w(line: str = "") -> None:
        L.append(line)

    w(f"// ============================================================================")
    w(f"// SCAFFOLD: {profile_name}")
    w(f"// Generated {today} by contracts/gen_scaffold.py from mqtt_contract.yaml")
    w(f"//")
    w(f"// HOW TO START:")
    w(f"//   1. Copy this directory to your PlatformIO project root.")
    w(f"//   2. Copy include/secrets.h.example → include/secrets.h, fill WiFi credentials.")
    w(f"//   3. Fill in the TODO sections below with your hardware logic.")
    w(f"//   4. Run: pio run -e {profile_name}-prod")
    w(f"//   5. Flash, connect Improv (or use hardcoded SSID), claim on portal.idryer.org.")
    w(f"//")
    w(f"// Capabilities: {caps_str}")
    w(f"// ============================================================================")
    w()
    w("#include <Arduino.h>")
    w("#include <WiFi.h>")
    w("#include <ArduinoJson.h>")
    w("#include <idryer_core.h>")
    w("#include <secrets.h>")
    w()
    w("// ── Config ────────────────────────────────────────────────────────────────────")
    w(f"// Flags generated from device_profiles.{profile_name} in mqtt_contract.yaml.")
    w("// After adding a new capability: edit mqtt_contract.yaml → run contracts/regen.sh.")
    w("static const idryer::Config CFG = {")
    w(f"    .deviceType        = idryer::DeviceType::{device_type},")
    w(f"    .unitsCount        = 1,")
    w(f"    // Peripheral capabilities:")
    for line in cap_flag_lines:
        w(line)
    w("    // Basic air sensors (set true if your hardware has them):")
    w("    .hasAirTemp        = false,  // TODO: SHT31, DHT22, etc.")
    w("    .hasAirHumidity    = false,")
    w("    .hasHeaterTemp     = false,")
    w("    // Cloud integrations (set true if you need them):")
    w("    .allowBambu        = false,")
    w("    .allowMoonraker    = false,")
    w("    .allowHa           = false,")
    w("    // Publish periods (ms):")
    w("    .telemetryPeriodMs = 5000,")
    w("    .statusPeriodMs    = 10000,")
    w("    // Identity shown on portal:")
    w('    .hardwareVersion   = "1.0",')
    w('    .firmwareVersion   = "0.1.0",')
    w(f'    .model             = "{profile_name}",')
    w("};")
    w()
    w("// ── IProfile implementation ──────────────────────────────────────────────────")
    w(f"class {class_name}Profile : public idryer::IProfile {{")
    w("public:")
    w("    void onOnline() override {")
    w("        // Called once when device reaches Online state.")
    w("        // TODO: load config from NVS, apply to hardware (pins, PWM, etc.).")
    w("    }")
    w()
    w("    void loop() override {")
    w("        // Called every IdryerRuntime::loop().")
    w("        // TODO: read sensors, fill telemetry, call s_runtime.publishTelemetry()")
    w("        //   static uint32_t t = 0;")
    w("        //   if (millis() - t > CFG.telemetryPeriodMs) {")
    w("        //       t = millis();")
    w("        //       idryer::Telemetry tel = {};")
    w("        //       tel.airTempC[0]       = myTempSensor.read();")
    w("        //       tel.airHumidityPct[0] = myHumSensor.read();")
    w("        //       s_runtime.publishTelemetry(tel);")
    w("        //   }")
    w("    }")
    w()
    w("    void getConfig(JsonDocument& out) override {")
    w("        // Snapshot of current config → published to idryer/{serial}/config.")
    w("        // TODO: serialize your menu/NVS state here.")
    w('        out["v"] = 1;')
    w("    }")
    w()
    w("    bool applyConfig(int id, int val) override {")
    w("        // Apply parameter from commands/set (id = menu item id, val = new value).")
    w("        // TODO: switch(id) { case MENU_ID_BRIGHTNESS: applyBrightness(val); break; }")
    w("        (void)id; (void)val;")
    w("        return true;")
    w("    }")
    w()
    w("    void buildInfoJson(char* buf, size_t len) const override {")
    w("        // Published to idryer/{serial}/info → portal reads capabilities,")
    w("        // builds DynamicCard widgets automatically.")
    w("        StaticJsonDocument<512> doc;")
    w(f'        doc["deviceType"]      = "{profile_name}";')
    w('        doc["firmwareVersion"] = CFG.firmwareVersion;')
    w('        doc["hardwareVersion"] = CFG.hardwareVersion;')
    for line in caps_json_lines:
        w(line)
    w("        char ts[32];")
    w("        idryer::MqttClient::getIsoTimestamp(ts);")
    w('        doc["timestamp"] = ts;')
    w("        serializeJson(doc, buf, len);")
    w("    }")
    w("};")
    w()
    w("// ── Platform layer ────────────────────────────────────────────────────────────")
    w("static idryer::ArduinoWifiStore       s_wifiStore;")
    w("static idryer::ArduinoWifiManager     s_wifi;")
    w("static idryer::ArduinoCredentialStore s_credentials;")
    w("static idryer::ArduinoHttpClient      s_http;")
    w()
    w("// ── Cloud stack ───────────────────────────────────────────────────────────────")
    w("static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);")
    w("static idryer::MqttClient               s_mqtt;")
    w("static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);")
    w("static idryer::ActionDispatcher         s_dispatcher;")
    w()
    w("// ── Product layer ─────────────────────────────────────────────────────────────")
    w(f"static {class_name}Profile   s_profile;")
    w("static idryer::IdryerRuntime  s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);")
    w()
    w("// ── Command handler ──────────────────────────────────────────────────────────")
    w("static void handleCommand(const char* cmd, JsonObjectConst data) {")
    w('    const char* action = data["action"] | "";')
    w()
    w('    if (strcmp(cmd, "get_config") == 0 ||')
    w('        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))')
    w("    {")
    w("        StaticJsonDocument<256> doc;")
    w("        s_profile.getConfig(doc);")
    w("        s_mqtt.publishConfig(doc);")
    w("        return;")
    w("    }")
    w('    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }')
    w('    if (strcmp(cmd, "set")    == 0) { s_dispatcher.handleSet(data);    return; }')
    w("    // TODO: add product-specific commands here:")
    w('    // if (strcmp(cmd, "my_command") == 0) { ... return; }')
    w("}")
    w()
    w("// ── ActionDispatcher callbacks ────────────────────────────────────────────────")
    w("static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {")
    for line in invoke_hint_lines:
        w(line)
    w("    (void)action; (void)args;")
    w("    return false;")
    w("}")
    w()
    w("static void onSet(JsonObjectConst data, void* /*ctx*/) {")
    w('    int id  = data["id"]  | -1;')
    w('    int val = data["val"] | -1;')
    w("    if (id < 0 || val < 0) return;")
    w("    s_profile.applyConfig(id, val);")
    w("}")
    w()
    w("// ── Setup / Loop ──────────────────────────────────────────────────────────────")
    w("void setup() {")
    w("    Serial.begin(115200);")
    w("    idryer::hal::initArduinoHal(&Serial);")
    w()
    w("    // WiFi: NVS credentials take priority over secrets.h defaults.")
    w("    char ssid[64], pass[64];")
    w("    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {")
    w("        s_wifi.begin(ssid, pass);")
    w("    } else {")
    w("        s_wifiStore.save(WIFI_SSID, WIFI_PASSWORD);")
    w("        s_wifi.begin(WIFI_SSID, WIFI_PASSWORD);")
    w("    }")
    w()
    w("    s_credentials.seedSerialFromMac();")
    w("    s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);")
    w()
    w("    s_dispatcher.setInvokeHandler(onInvoke, nullptr);")
    w("    s_dispatcher.setSetCallback(onSet, nullptr);")
    w()
    w("    s_runtime.setCommandHandler(handleCommand);")
    w("    s_runtime.begin();")
    w("}")
    w()
    w("void loop() {")
    w("    s_runtime.loop();")
    w("}")

    return "\n".join(L) + "\n"


def gen_platformio_ini(profile_name: str) -> str:
    return textwrap.dedent(f"""\
        ; PlatformIO config for {profile_name}
        ; Generated by contracts/gen_scaffold.py
        ;
        ; Before building:
        ;   1. Set upload_port / monitor_port for your device.
        ;   2. Adjust STORAGE_LED_PIN / STORAGE_I2C_* if applicable.

        [platformio]
        default_envs = {profile_name}-prod

        [env]
        platform  = espressif32
        board     = esp32-c3-devkitm-1
        framework = arduino
        upload_port   = /dev/cu.usbmodem101
        monitor_port  = /dev/cu.usbmodem101
        monitor_speed = 115200
        monitor_raw   = yes

        lib_deps =
          bblanchon/ArduinoJson @ ^6.21.3
          knolleary/PubSubClient @ ^2.8
          https://github.com/jnthas/Improv-WiFi-Library.git
          links2004/WebSockets @ ^2.4.0

        [flags_usb_cdc]
        build_flags =
          -DARDUINO_USB_MODE=1
          -DARDUINO_USB_CDC_ON_BOOT=1
          -DDEBUG_SERIAL=Serial
          -Iinclude

        [flags_prod]
        build_flags =
          -DCORE_DEBUG_LEVEL=0
          -DIDRYER_API_BASE=\\"https://portal.idryer.org/api\\"
          -DMQTT_BROKER=\\"mqtt.idryer.org\\"
          -DMQTT_PORT=8883
          -DMQTT_USE_TLS=1

        [flags_stage]
        build_flags =
          -DCORE_DEBUG_LEVEL=3
          -DIDRYER_API_BASE=\\"https://staging.idryer.org/api\\"
          -DMQTT_BROKER=\\"staging.idryer.org\\"
          -DMQTT_PORT=1884
          -DMQTT_USE_TLS=0

        [{profile_name}-prod]
        extra_scripts =
          pre:../../idryer-core/extra_scripts/pre_gen_menu.py
        build_flags =
          ${{flags_usb_cdc.build_flags}}
          ${{flags_prod.build_flags}}

        [{profile_name}-stage]
        extra_scripts =
          pre:../../idryer-core/extra_scripts/pre_gen_menu.py
        build_flags =
          ${{flags_usb_cdc.build_flags}}
          ${{flags_stage.build_flags}}
          -DIDRYER_DEV_REPL=1
        """)


def gen_secrets_h() -> str:
    return textwrap.dedent("""\
        // secrets.h — copy to include/secrets.h and fill in your values.
        // Keep this file OUT of git (.gitignore: include/secrets.h).
        #pragma once

        #define WIFI_SSID     "your-ssid"
        #define WIFI_PASSWORD "your-password"

        #ifndef IDRYER_API_BASE
        #define IDRYER_API_BASE "https://portal.idryer.org/api"
        #endif
        """)


def gen_readme(profile_name: str, capabilities: list[str]) -> str:
    caps_str = ", ".join(capabilities) if capabilities else "none"
    return textwrap.dedent(f"""\
        # {profile_name}

        Auto-generated scaffold. Capabilities: **{caps_str}**.

        ## Quick start

        1. Copy `include/secrets.h.example` → `include/secrets.h`, fill WiFi credentials.
        2. Open in VS Code with PlatformIO extension.
        3. Fill `TODO` sections in `src/main.cpp` with your hardware logic.
        4. Build and flash: `pio run -e {profile_name}-prod --target upload`
        5. Claim the device on [portal.idryer.org](https://portal.idryer.org).

        ## Adding a new capability

        1. Add entry to `capability_vocabulary` in `contracts/mqtt_contract.yaml`.
        2. Run `cd contracts && bash regen.sh`.
        3. Set the new `has*` flag in `CFG` inside `src/main.cpp`.
        4. Flash the device — the portal picks up the new capability from `/info`.

        ## Business logic

        Business logic (e.g. "if temp > 45 turn on fan") goes in the `loop()` method
        of `{_to_class_name(profile_name)}Profile`. The yaml only describes the interface
        (what is published/accepted), not the device's internal behaviour.

        If you want the threshold to be user-configurable from the portal, expose it as
        a menu item and read it from NVS in `applyConfig()`.
        """)


# ── Main ─────────────────────────────────────────────────────────────────────

def generate_profile(profile_name: str, profile: dict, doc: dict, out_root: Path) -> None:
    capabilities: list[str] = profile.get("capabilities") or []

    out_dir = out_root / profile_name
    (out_dir / "src").mkdir(parents=True, exist_ok=True)
    (out_dir / "include").mkdir(parents=True, exist_ok=True)

    files = {
        out_dir / "src" / "main.cpp":           gen_main_cpp(profile_name, capabilities, doc),
        out_dir / "platformio.ini":              gen_platformio_ini(profile_name),
        out_dir / "include" / "secrets.h.example": gen_secrets_h(),
        out_dir / "README.md":                   gen_readme(profile_name, capabilities),
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
    print("   Copy the folder you need to your PlatformIO project.")
    print("   Then: cp include/secrets.h.example include/secrets.h")


if __name__ == "__main__":
    main()
