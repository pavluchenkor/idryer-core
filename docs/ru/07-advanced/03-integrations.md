# Интеграции с принтерами

Модуль интеграций позволяет устройству iDryer/iHeater подключаться к сторонним системам: Home Assistant, Bambu Lab (LAN), Moonraker/Klipper. Подключается отдельно:

```cpp
#include <idryer_integrations.h>
```

**Интеграции — опциональный модуль.** Storage Link их не использует. Они реализованы для iDryer LINK и iHeater LINK.

## LinkIntegrationsManager

Главный класс модуля. Управляет одной активной интеграцией одновременно. Подключается через продуктовый `CommandHandler` — тот же обработчик, что используется для MQTT и локального WS.

```cpp
LinkIntegrationsStore intStore;
idryer::cloud::LinkIntegrationsManager intManager(&s_mqtt, &intStore);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "link_integration") == 0) {
        intManager.handleLinkIntegrationCommand(data); return;
    }
    if (strcmp(cmd, "bambu_apply") == 0) {
        intManager.handleBambuApplyCommand(data); return;
    }
    // ... другие команды продукта ...
}

// в setup():
runtime.setCommandHandler(handleCommand);
local.setCommandSink(handleCommand);
intManager.begin(); // после runtime.begin()
// в loop(): intManager.loop();
```

Менеджер хранит конфигурации всех трёх интеграций в NVS через `LinkIntegrationsStore`. Переключение активной интеграции выполняется командой:

```json
{"active": "bambu"}     // или "ha", "moonraker", "none"
```

Состояние публикуется в `idryer/{serial}/integrations/status` (retained) при изменении и каждые 30 секунд.

## Bambu Lab

`BambuClient` подключается к принтеру по MQTT в локальной сети (TLS, порт 8883, self-signed cert, `setInsecure`).

Два режима работы в зависимости от устройства:

| Режим | DeviceType | Поведение |
|-------|-----------|-----------|
| **Writer** | Dryer | отправляет `ams_filament_setting` в принтер при `bambu_apply` |
| **Reader** | Heater / IHeaterLink | подписывается на `device/{printerSerial}/report`, передаёт статус принтера в колбэк |

Параметры подключения:

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

Reconnect с exponential backoff от 1 с до 60 с.

Колбэки:

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — подключение к HA MQTT-брокеру (не к облаку HA, а к встроенному MQTT-серверу HA).

Настраивается через команду `link_integration`:

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

Адаптер поддерживает mDNS-обнаружение хоста (строка `homeassistant.local`) и прямое IP-подключение. Reconnect с backoff.

`HaMqttClient` предоставляется наружу через `intManager.haMqttClient()` — продукт может публиковать сущности HA через него.

Для устройства необходимо установить client ID:

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` подключается через WebSocket (`ws://host:port/websocket`) и использует JSON-RPC 2.0 для подписки на объекты Klipper.

Основное применение — iHeater: получение целевой температуры камеры через `gcode_macro VIRTUAL_CHAMBER`.

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

Клиент подписывается на объекты Klipper включая `gcode_macro VIRTUAL_CHAMBER`, `print_stats`, `display_status`, температурные сенсоры.

Колбэки:

```cpp
intManager.setVirtualChamberCallback([](const VirtualChamberData& vc) {
    // vc.target — целевая температура камеры
    // vc.available — объект VIRTUAL_CHAMBER виден в Klipper
});

intManager.setMoonrakerStatusCallback([](const MoonrakerStatus& s) {
    // s.printerState, s.nozzleTemp, s.progress, ...
});
```

## Жизненный цикл колбэков

Колбэк (`setVirtualChamberCallback`, `setBambuPrinterStatusCallback`, `setMoonrakerStatusCallback`, `setPrinterStatusCallback` у `BambuClient`) — это **подписка**, а не немедленный вызов. Регистрация только сохраняет указатель в менеджере; сам вызов произойдёт позже, из `intManager.loop()`, когда от принтера придут новые данные.

Полный путь от принтера до продукта:

```
1. Внешний принтер шлёт данные         (Bambu MQTT / Klipper WS)
2. intManager.loop()                   (вызывается из вашего main.loop())
   └── moonrakerClient.loop() / bambuClient.loop()
       └── парсит JSON
           └── собирает структуру (VirtualChamberData / BambuPrinterStatus)
               └── вызывает зарегистрированный колбэк
                   └── ваш код (применить target, обновить состояние, ...)
```

Что это значит для продукта:

- Колбэк должен быть **зарегистрирован до** того, как менеджер начнёт работать. Регистрировать после `intManager.begin()` тоже допустимо, но ранее пришедшие данные потеряются.
- Колбэк вызывается **из контекста `loop()`**, не из прерывания и не из отдельного таска (если продукт сам не запустил его в таске). Можно безопасно работать с любыми библиотеками, рассчитанными на основной поток.
- Если колбэк не зарегистрирован — данные просто отбрасываются. Нет очереди.
- Колбэк — это `std::function`, поэтому подходит как обычная функция, так и лямбда с захватом контекста (`this`, локальные указатели).

Пример типичной связки в продукте:

```cpp
// 1. Привязать продуктовую логику к железу один раз.
iheaterlink::wireAutoHeat(&s_output);

// 2. Подписать колбэк — менеджер вызовет его при каждом обновлении.
intManager.setVirtualChamberCallback(iheaterlink::onVirtualChamberUpdate);

// 3. В loop() — менеджер сам всё прокрутит.
void loop() {
    runtime.loop();
    intManager.loop();
}
```

Когда `Klipper` поднимает `gcode_macro VIRTUAL_CHAMBER` с `target=55`, через секунду-две (зависит от polling-интервала) вызовется `onVirtualChamberUpdate(data)` с `data.target=55`. Внутри функции продукт применяет команду на свою периферию.

## Ограничения

- Одна активная интеграция одновременно. Переключение — атомарное: старая останавливается, новая запускается.
- Один экземпляр `BambuClient` на устройство (singleton через статический указатель).
- `LinkIntegrationsStore` хранит конфигурацию в NVS — настройки сохраняются через перезагрузку.
- Устройство должно указать свой тип (`setDeviceType`) для корректного выбора режима Bambu:
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // или Heater, IHeaterLink
  ```
