---
title: "Запустить устройство на idryer-core за 5 минут"
description: "Первое устройство на idryer-core: проект PlatformIO, минимальная прошивка, Wi-Fi и привязка к аккаунту в приложении iDryer."
---

# Запустить устройство на idryer-core за 5 минут

После этой страницы ESP32 будет в сети, привязан к вашему аккаунту и виден на портале [portal.idryer.org](https://portal.idryer.org/) и в приложении iDryer. Понадобятся: плата ESP32-C3 (DevKit, Super Mini или совместимая), USB-кабель, PlatformIO в VS Code, телефон с приложением iDryer, сеть Wi-Fi 2,4 ГГц.

## 1. Проект PlatformIO

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← копия, git submodule или символическая ссылка
└── src/
    └── main.cpp
```

`platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP — транспорт ESP8266 из зависимостей espMqttClient: на ESP32 не собирается.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Плата без USB-UART (ESP32-C3 SuperMini): Serial — по USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

Библиотеки ядра (MQTT, ArduinoJson, WebSockets, Improv) приходят сами из `library.json` ядра. Для платы с USB-UART последние два флага не нужны; `board` — под вашу плату.

## 2. Код

Скопируйте в `src/main.cpp` пример [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino):

```cpp
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
```

В прошивке нет ни пароля от сети, ни данных аккаунта: сеть и привязку устройство получит из приложения.

## 3. Прошить и открыть лог

```bash
pio run -t upload
pio device monitor -b 115200
```

Пока у устройства нет Wi-Fi, лог молчит: ядро держит порт для Improv. LED часто мигает — устройство ждёт сети и привязки.

## 4. Wi-Fi и привязка в приложении

1. Подключите телефон к той сети Wi-Fi, в которой будет работать устройство, и войдите в приложение iDryer под своим аккаунтом портала.
2. На главном экране нажмите **Подключить новое устройство** — откроется шаг **Wi-Fi**.
3. Проверьте название сети, введите пароль и нажмите **Подключить устройство**. Приложение передаёт настройки до 90 секунд; когда устройство подключится, появится **Устройство подключено**. Нажмите **Далее**.
4. На шаге **Привязка** нажмите **Привязать**. Приложение найдёт устройство в сети, получит у портала одноразовый токен привязки и передаст его устройству.
5. После **Устройство привязано** оно появится в списке устройств на портале и в приложении.

В логе после подключения к сети:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

После привязки:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Проверка

- на портале и в приложении устройство в сети;
- на карточке — температура чипа ESP32;
- LED мигает раз в секунду. Горит или не горит ровно — нет связи с порталом.

## Если не получилось

- приложение не дождалось подключения — проверьте пароль и что сеть 2,4 ГГц; при неверном пароле устройство снова ждёт настроек. Другие способы передать сеть — [Wi-Fi](01-wifi.md);
- на шаге **Привязка** устройство не нашлось — телефон и устройство должны быть в одной сети; гостевые сети часто блокируют обнаружение устройств. Подробнее — [Привязка к аккаунту](02-claim.md);
- сборка и флаги — [Подробная настройка](99-detailed-setup.md).

## Что дальше

- [Телеметрия](03-telemetry.md) — датчик и своя величина на карточке.
- [Примеры ядра](https://github.com/pavluchenkor/idryer-core/tree/main/examples) — действия карточки, свои датчики и контролы.
