// ============================================================================
//  01_blink_status — самый простой пример idryer-core.
// ============================================================================
//
// Что показывает:
//   - устройство на iDryer::Link: Wi-Fi, привязка к аккаунту, MQTT и доступ
//     по локальной сети поднимает ядро, в коде — только описание устройства;
//   - телеметрию: температура чипа ESP32 попадает в ячейку температуры
//     карточки на портале и в приложении;
//   - состояние на встроенном LED: часто мигает — ждёт привязки в
//     приложении, редко мигает — онлайн, горит или не горит ровно — нет
//     связи с порталом.
//
// Что настроить: platformio.ini — см. examples/README.md.
//
// Wi-Fi и привязка — не в коде. Сеть передаёт приложение iDryer (ESPTouch)
// или веб-установщик по USB (Improv), токен привязки — приложение.
// Отвязка в приложении или на портале приходит командой revoke.
//
// Common pitfalls:
//   - ESP32 не работает с сетями 5 ГГц.
//   - В loop() нельзя вызывать длинный delay(): s_link.loop() держит связь.
//   - Имя объекта не `link`: так называется функция POSIX, будет конфликт.
//   - На части плат LED включается низким уровнем — тогда мигание инверсное.
// ============================================================================

#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // своё устройство
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Период мигания по состоянию; 0 — не мигать.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Отвязка: ядро стирает секрет и снова ждёт привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // Поля телеметрии ядро публикует само, раз в 30 с (в простое — раз в 60 с).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
