# Шаг 01 — WiFi-подключение через Improv

После этого шага ваш ESP32 будет подключён к WiFi, а credentials сохранятся в NVS для автоматического подключения при следующей перезагрузке. Портал и MQTT — в следующем шаге.

## Что понадобится

**Железо:**

- Плата ESP32-C3 (DevKit, Super Mini или совместимая)
- USB-кабель (USB-C или Micro-USB — зависит от платы)

**ПО:**

- PlatformIO в VS Code
- Браузер Chrome или Edge (Web Serial API не работает в Safari и Firefox)

## Шаги

**1. Создать `platformio.ini`** в корне проекта:

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Замените `board` на значение вашей платы (`esp32-c3-devkitm-1`, `seeed_xiao_esp32c3` и т.д.).

**2. Скопировать пример.** Возьмите содержимое [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) и сохраните как `src/main.cpp` в вашем проекте.

**3. Указать ChipFamily.** В скопированном файле найдите строку:

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

Убедитесь, что ChipFamily совпадает с вашим чипом: `CF_ESP32_C3`, `CF_ESP32_S3` или `CF_ESP32`.

**4. Прошить:**

```bash
pio run -e improv-demo -t upload
```

**5. Открыть [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)** в Chrome или Edge. Нажмите **Connect** и выберите USB-порт устройства в диалоге браузера.

**6. Ввести SSID и пароль** своей сети 2.4 GHz. Веб-страница передаст credentials на плату по Serial-Improv. Плата сохранит их в NVS.

## Проверка

Откройте Serial Monitor:

```bash
pio device monitor -b 115200
```

После успешного подключения в логе появится:

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

Если этой строки нет — перейдите в раздел «Что дальше», там — ссылка на troubleshooting.

!!! note
    Если credentials уже сохранены в NVS с предыдущего запуска, плата подключится к WiFi при boot автоматически — без Improv.

## Что дальше

- [02-claim.md](02-claim.md) — привязать устройство к порталу idryer.org.
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — если WiFi не подключается.
