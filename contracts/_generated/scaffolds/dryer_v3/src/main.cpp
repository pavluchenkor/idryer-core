// ============================================================================
// SCAFFOLD: dryer_v3
// Generated 2026-09-23 by contracts/gen_scaffold.py from mqtt_contract.yaml
//
// HOW TO START:
//   1. Copy this directory; put idryer-core into lib/idryer-core
//      (a copy, a git submodule or a symbolic link).
//   2. Fill in the TODO sections with your hardware logic.
//   3. pio run -e dryer_v3-prod -t upload
//   4. Wi-Fi and pairing: the iDryer app, "Connect a new device".
//
// Capabilities: heater, fan, weight, rfid, air_temp, air_humidity, heater_temp, servo
// Unit modes:   DRYING, STORAGE, PROFILE
// ============================================================================

#include <Arduino.h>
#include <iDryer.h>

// Flags from device_profiles.dryer_v3 in mqtt_contract.yaml.
static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Dryer,
    .unitsCount      = 1,   // TODO: number of units (chambers)
    .hasHeater       = true,   // Управляемый нагреватель (targetTempC, durationS)
    .hasFan          = true,   // Вентилятор (on/off)
    .hasWeight       = true,   // Датчик веса (граммы филамента)
    .hasRfid         = true,   // RFID-ридер метки катушки
    .hasAirTemp      = true,   // Датчик температуры воздуха (SHT/DHT)
    .hasAirHumidity  = true,   // Датчик влажности воздуха
    .hasHeaterTemp   = true,   // Датчик температуры корпуса нагревателя
    .hasServo        = true,   // Сервопривод заслонки (открыта/закрыта)
    .otaInterrupt    = iDryer::OTA_INTERRUPT_DRYER_V3,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "dryer_v3",
};
static iDryer::Link s_link(CFG);

// Card action: start. Numbers in args are already clamped to the limits.
static void onStart(uint8_t unit, JsonObjectConst args) {
    // TODO: start the hardware with args.
    (void)args;
    s_link.status.mode[unit] = iDryer::UnitMode::Drying;
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
    card.action("start", "DRYING", onStart)
        .param("temperature", "target_temperature", 30, 90, 1, 50, "°C")
        .param("duration", "duration", 0, 2880, 10, 240, "min");
    card.action("stop", "IDLE", onStop);
    // More on the card: docs/*/09-add-product/02-add-widget.md
}

void loop() {
    s_link.loop();

    // TODO: read the hardware; the core publishes telemetry itself.
    // NAN in a float field means no data: the field is not sent.
    // s_link.telemetry.heaterPower01[0] = ...;   // heater
    // s_link.telemetry.fanOn[0] = ...;   // fan
    // s_link.telemetry.airTempC[0] = ...;   // air_temp
    // s_link.telemetry.airHumidityPct[0] = ...;   // air_humidity
    // s_link.telemetry.heaterTempC[0] = ...;   // heater_temp
    // s_link.telemetry.servoOpen[0] = ...;   // servo
}
