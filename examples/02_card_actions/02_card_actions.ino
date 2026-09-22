// ============================================================================
//  02_card_actions — действия карточки: запуск с параметрами и остановка.
// ============================================================================
//
// Что показывает:
//   - действия card-манифеста: «Нагрев» с параметрами температура и время,
//     «Стоп», вызов прибора (кнопка в шапке карточки мигает LED);
//   - status: режим, уставка, длительность, прошедшее время. По режиму юнита
//     карточка сама выбирает, что показать: форму запуска или блок сессии
//     с кнопкой «Стоп»;
//   - телеметрию: температура камеры и мощность нагревателя.
//   Железа не нужно: камера смоделирована, «нагреватель» — встроенный LED.
//
// Когда читать: после 01_blink_status.
//
// Что настроить: platformio.ini — см. examples/README.md.
//
// Common pitfalls:
//   - Колбэки команд — функции без захвата: лямбда с захватом не
//     превращается в указатель на функцию.
//   - Числа в args уже зажаты в пределы параметра, отсутствующие заменены
//     значением по умолчанию — повторно проверять не нужно.
//   - После смены режима вызывайте publishStatusNow(): карточка
//     переключается по статусу.
//   - duration приходит в минутах, 0 — без ограничения.
// ============================================================================

#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // ячейка мощности нагрева
    .hasAirTemp      = true,    // ячейка температуры
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Card Actions",
};
static iDryer::Link s_link(CFG);

static constexpr float kRoomC = 25.0f;
static float    s_airC       = kRoomC;
static uint32_t s_startMs    = 0;
static uint8_t  s_identifyN  = 0;   // сколько раз ещё мигнуть по вызову

static void stopUnit(uint8_t unit) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopUnit(unit); }

static void onIdentify(uint8_t, JsonObjectConst) { s_identifyN = 6; }

// Раз в секунду: модель камеры, прошедшее время, конец сессии.
static void tick() {
    const bool heating  = s_link.status.mode[0] == iDryer::UnitMode::Heating;
    const bool heaterOn = heating && s_airC < s_link.status.targetTempC[0];

    // Нагреватель даёт +0.5 °C/с, камера остывает к комнатной.
    s_airC += (heaterOn ? 0.5f : 0.0f) - (s_airC - kRoomC) * 0.01f;
    s_link.telemetry.airTempC[0]      = s_airC;
    s_link.telemetry.heaterPower01[0] = heaterOn ? 1.0f : 0.0f;

    if (heating) {
        const uint32_t elapsed = (millis() - s_startMs) / 1000;
        s_link.status.elapsedS[0] = elapsed;
        const uint32_t dur = s_link.status.durationS[0];
        if (dur && elapsed >= dur) stopUnit(0);
    }

    if (s_identifyN) {
        s_identifyN--;
        digitalWrite(LED_PIN, s_identifyN % 2);
    } else {
        digitalWrite(LED_PIN, heaterOn);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // Режим после действия — строка status.units[].mode.
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 80, 1, 50, "°C")
        .param("duration", "duration", 0, 720, 10, 60, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
    // Без смены режима: постоянная кнопка шапки «вызвать прибор».
    card.action("identify", nullptr, onIdentify)
        .deviceClass("identify");

    s_link.every(1000, tick);
}

void loop() {
    s_link.loop();
}
