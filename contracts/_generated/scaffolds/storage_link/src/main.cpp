// ============================================================================
// SCAFFOLD: storage_link
// Generated 2026-09-22 by contracts/gen_scaffold.py from mqtt_contract.yaml
//
// HOW TO START:
//   1. Copy this directory; put idryer-core into lib/idryer-core
//      (a copy, a git submodule or a symbolic link).
//   2. Fill in the TODO sections with your hardware logic.
//   3. pio run -e storage_link-prod -t upload
//   4. Wi-Fi and pairing: the iDryer app, "Connect a new device".
//
// Capabilities: led, air_temp, air_humidity
// Unit modes:   LIGHT_ANIMATION
// ============================================================================

#include <Arduino.h>
#include <iDryer.h>

// Flags from device_profiles.storage_link in mqtt_contract.yaml.
static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::StorageLink,
    .unitsCount      = 1,   // TODO: number of units (chambers)
    .hasLed          = true,   // Адресная LED-лента
    .hasAirTemp      = true,   // Датчик температуры воздуха (SHT/DHT)
    .hasAirHumidity  = true,   // Датчик влажности воздуха
    .otaInterrupt    = iDryer::OTA_INTERRUPT_STORAGE_LINK,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "storage_link",
};
static iDryer::Link s_link(CFG);

static const char* const kEffects[] = { "solid", "breathe" };

// Card action: start. Numbers in args are already clamped to the limits.
static void onStart(uint8_t unit, JsonObjectConst args) {
    // TODO: start the hardware with args.
    (void)args;
    s_link.status.mode[unit] = iDryer::UnitMode::LightAnimation;
    s_link.publishStatusNow();
}

// Card action: stop.
static void onStop(uint8_t unit, JsonObjectConst) {
    // TODO: stop the hardware.
    s_link.status.mode[unit] = iDryer::UnitMode::Idle;
    s_link.publishStatusNow();
}

void setup() {
    s_link.begin();
    // Unlinking in the app or on the portal: erase the secret, wait for pairing.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    // Card actions: the mode after the action and its start parameters.
    // TODO: limits of your device.
    auto& card = s_link.card();
    card.action("start", "LIGHT_ANIMATION", onStart)
        .select("effect", "effect", kEffects, 2, "solid")
        .color("color", "rgb_color", "#FFFFFF");
    card.action("stop", "IDLE", onStop);
    // More on the card: docs/*/09-add-product/02-add-widget.md
}

void loop() {
    s_link.loop();

    // TODO: read the hardware; the core publishes telemetry itself.
    // NAN in a float field means no data: the field is not sent.
    // s_link.telemetry.airTempC[0] = ...;   // air_temp
    // s_link.telemetry.airHumidityPct[0] = ...;   // air_humidity
}
