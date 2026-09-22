// ============================================================================
//  03_card_entities — свои датчики и контролы на карточке.
// ============================================================================
//
// Что показывает:
//   - свой датчик: значение добавляется в телеметрию и объявляется сенсором
//     с JSON-путём — карточка показывает его без кода на портале;
//   - двоичный датчик: кнопка BOOT на плате (нажата / отпущена);
//   - простые контролы — кнопка, число, список: значение уходит сразу, как
//     его изменили, ядро вызывает колбэк;
//   - разметку карточки: какие сущности стоят в одном ряду.
//
// Когда читать: после 02_card_actions. Действия с параметрами запуска и
// режимом — там; здесь — то, что меняется одним нажатием.
//
// Что настроить: platformio.ini — см. examples/README.md.
//
// Common pitfalls:
//   - Путь сенсора указывает на поле в телеметрии: "units[0].heapKb" —
//     поле heapKb первого юнита. Поле добавляется в onTelemetryPublish.
//   - select: до 6 вариантов по 15 символов.
//   - Колбэки — функции без захвата, состояние — в глобальных переменных.
// ============================================================================

#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif
#ifndef BOOT_PIN
#define BOOT_PIN 9  // кнопка BOOT на ESP32-C3
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Card Entities",
};
static iDryer::Link s_link(CFG);

static const char* const kPatterns[] = { "steady", "blink", "off" };

static int         s_brightness = 50;          // %
static const char* s_pattern    = kPatterns[0];
static uint8_t     s_flashN     = 0;           // сколько раз ещё мигнуть

static void onBrightness(float v) { s_brightness = (int)v; }

static void onPattern(const char* opt) {
    for (const char* p : kPatterns) {
        if (strcmp(p, opt) == 0) s_pattern = p;
    }
}

static void onFlash() { s_flashN = 6; }

static void drawLed() {
    static uint32_t lastMs = 0;
    if (millis() - lastMs < 250) return;
    lastMs = millis();

    bool on;
    if (s_flashN) {
        s_flashN--;
        on = s_flashN % 2;
    } else if (strcmp(s_pattern, "blink") == 0) {
        on = (millis() / 500) % 2;
    } else {
        on = strcmp(s_pattern, "steady") == 0;
    }
    analogWrite(LED_PIN, on ? s_brightness * 255 / 100 : 0);
}

void setup() {
    Serial.begin(115200);
    pinMode(BOOT_PIN, INPUT_PULLUP);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    // Свои поля — в телеметрию первого юнита перед каждой публикацией.
    s_link.onTelemetryPublish([](JsonObject root) {
        root["units"][0]["heapKb"] = ESP.getFreeHeap() / 1024;
        root["units"][0]["boot"]   = digitalRead(BOOT_PIN) == LOW;
    });

    auto& card = s_link.card();
    card.sensor("heap", "Free heap", "KB", "units[0].heapKb");
    card.binarySensor("boot", "BOOT button", "units[0].boot");
    card.number("brightness", "Brightness", 0, 100, 5, "%", onBrightness);
    card.select("pattern", "Pattern", kPatterns, 3, onPattern);
    card.button("flash", "Flash", onFlash);

    // Разметка: ряды по id сущностей.
    card.layoutRow("heap", "boot");
    card.layoutRow("brightness", "pattern");
    card.layoutRow("flash");
}

void loop() {
    s_link.loop();
    drawLed();
}
