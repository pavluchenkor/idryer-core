# Что такое idryer-core

Если вы делаете ESP32-устройство для облака iDryer, эта библиотека берёт на себя WiFi-provisioning (Improv), claim-протокол, MQTT-сессию (TLS, reconnect, time-sync), периодическую публикацию телеметрии/статуса и маршрутизацию входящих команд. Примерно 500 строк boilerplate сворачиваются в `link.begin(); link.loop();`.

## Минимальный пример

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## Что библиотека делает

- WiFi-подключение и удержание соединения; Improv-provisioning через Web Serial для первой настройки.
- Claim-протокол: регистрация устройства в бэкенде, привязка к аккаунту пользователя через PIN.
- MQTT-сессия с брокером iDryer: TLS, persistent session, авто-reconnect, NTP time-sync.
- Периодическая публикация телеметрии (`Telemetry`) и статуса (`Status`) по таймеру.
- Маршрутизация входящих команд (`commands/invoke`, `commands/set`, `commands/ping`) к обработчику продукта.
- Local WebSocket-сервер: LAN-клиент видит тот же поток, что и облако.
- NVS-персистентность: WiFi credentials, device token, конфигурация меню между перезагрузками.

## Что библиотека не делает

- Не управляет железом продукта: вентиляторами, нагревателями, LED-лентами, датчиками.
- Не содержит бизнес-логику сушки, хранения или подсветки.
- Не знает о конкретных параметрах меню продукта — только транспортирует их.
- Не публикует телеметрию без данных от продукта: вы сами заполняете `link.telemetry.*` в `loop()`.

## Куда дальше

- [Запустить за 5 минут](../02-quickstart/01-five-minutes.md)
- [Полный API: iDryer::Link](../03-public-api/01-link-api-reference.md)
