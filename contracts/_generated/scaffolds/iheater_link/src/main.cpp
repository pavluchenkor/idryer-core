// ============================================================================
// SCAFFOLD: iheater_link
// Generated 2026-09-23 by contracts/gen_scaffold.py from mqtt_contract.yaml
//
// HOW TO START:
//   1. Copy this directory; put idryer-core into lib/idryer-core
//      (a copy, a git submodule or a symbolic link).
//   2. Fill in the TODO sections with your hardware logic.
//   3. pio run -e iheater_link-prod -t upload
//   4. Wi-Fi and pairing: the iDryer app, "Connect a new device".
//
// Capabilities: heater, fan
// Unit modes:   HEATING
// ============================================================================

#include <Arduino.h>
#include <iDryer.h>

// Flags from device_profiles.iheater_link in mqtt_contract.yaml.
static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::IHeaterLink,
    .unitsCount      = 1,   // TODO: number of units (chambers)
    .hasHeater       = true,   // Управляемый нагреватель (targetTempC, durationS)
    .hasFan          = true,   // Вентилятор (on/off)
    .otaInterrupt    = iDryer::OTA_INTERRUPT_IHEATER_LINK,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "iheater_link",
};
static iDryer::Link s_link(CFG);

// Card action: start. Numbers in args are already clamped to the limits.
static void onStart(uint8_t unit, JsonObjectConst args) {
    // TODO: start the hardware with args.
    (void)args;
    s_link.status.mode[unit] = iDryer::UnitMode::Heating;
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
    card.action("start", "HEATING", onStart)
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop);
    // More on the card: docs/*/09-add-product/02-add-widget.md
}

void loop() {
    s_link.loop();

    // TODO: read the hardware; the core publishes telemetry itself.
    // NAN in a float field means no data: the field is not sent.
    // s_link.telemetry.heaterPower01[0] = ...;   // heater
    // s_link.telemetry.fanOn[0] = ...;   // fan
}
