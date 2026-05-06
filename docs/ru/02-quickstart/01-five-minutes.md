# Запустить за 5 минут

После этой страницы ваш ESP32 будет прошит, подключится к WiFi и появится в [portal.idryer.org](https://portal.idryer.org/) со статусом Online. Потребуется: ESP32-C3 (DevKit, Super Mini или совместимый), USB-кабель, PlatformIO в VS Code.

## 1. Подготовить secrets.h

Скопируйте файл [`examples/secrets.h.example`](../../../../../examples/secrets.h.example) в `include/secrets.h` вашего проекта и укажите SSID и пароль своей WiFi-сети (только 2.4 GHz):

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

Добавьте `include/secrets.h` в `.gitignore`.

## 2. Настроить platformio.ini

Создайте `platformio.ini` в корне проекта:

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://path/to/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

Измените `board` под вашу плату. Замените `path/to/idryer-core` на реальный путь к библиотеке.

## 3. Скопировать пример 01_blink_status

Скопируйте содержимое [`examples/01_blink_status/01_blink_status.ino`](../../../../../examples/01_blink_status/01_blink_status.ino) в `src/main.cpp` вашего проекта. Пример не требует датчиков и дополнительных зависимостей — только минимальный composition root.

## 4. Прошить

```bash
pio run -e blink-demo -t upload
```

## 5. Открыть Serial Monitor

```bash
pio device monitor -b 115200
```

Ожидаемая последовательность в логе:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

После ввода PIN в портале (шаг 6):

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Если устройство остановилось на сообщении `PIN: ...` — это нормально, переходите к шагу 6.

## 6. Привязать устройство в портале

Откройте [portal.idryer.org](https://portal.idryer.org/), перейдите в раздел **Add device** и введите PIN из Serial Monitor. После успешного claiming устройство перейдёт в `Online`, встроенный LED начнёт моргать раз в 500 мс.

Подробно про привязку: [Onboarding](02-onboarding.md).

## Что дальше

- Добавить датчик — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- Добавить периферию — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- Полный справочник API — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Как работает изнутри — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
