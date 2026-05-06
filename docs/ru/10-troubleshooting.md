# Troubleshooting

Типовые симптомы при работе с `idryer-core`, их причины и решения.

Перед чтением убедитесь, что у вас включены HAL-логи (`idryer::hal::initArduinoHal(&Serial)`) и в `platformio.ini` стоит `-DCORE_DEBUG_LEVEL=3` или выше.

## WiFi

### Стейт-машина застряла в `WifiConnecting`

Симптомы: лог повторяется `state: WifiConnecting`, переход в `Provisioning` не наступает.

Возможные причины:

- Неверный SSID/пароль. Проверьте `WIFI_SSID` / `WIFI_PASSWORD` в `secrets.h`. После Improv credentials берутся из NVS — не из `secrets.h`.
- Сеть 5 GHz. ESP32 поддерживает только 2.4 GHz.
- Скрытая сеть или MAC-фильтр на роутере.
- `WiFi.begin()` вызван до `idryer::hal::initArduinoHal(...)` — лог не пишется, но это не причина зависания, а просто слепота.

Что проверить:

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### `WiFi` подключается, но через 30–60 секунд отваливается

Обычно: слабый сигнал (`RSSI < -80 dBm`), питание ESP32-C3 от USB-хаба без отдельного 5V/1A, конфликт с FreeRTOS-задачами.

Логирование RSSI в product loop:

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## Provisioning и claiming

### Стейт-машина застряла в `Provisioning`

Симптомы: `state: Provisioning` без перехода в `Registering` или `AwaitingClaim`.

Причины:

- Неверный `IDRYER_API_BASE` в build_flags. Должен быть `https://portal.idryer.org/api` (production) или `https://staging.idryer.org/api` (staging).
- Нет TLS-сертификата (Let's Encrypt ISRG Root X1). Встроен в `root_ca.h`, но при сборке без `MQTT_USE_TLS` HTTP-клиент тоже использует TLS — это нормально, корневой CA нужен и для HTTP API.
- Время устройства не синхронизировано (TLS handshake требует валидной даты). Проверьте, что `configTime(...)` вызывается в `setStateChangeCallback` после `WifiConnecting` (как в Storage Link).

### Стейт-машина застряла в `AwaitingClaim`

Это нормальное состояние, пока пользователь не ввёл PIN в портале. PIN печатается в лог через `setClaimPinCallback`.

Если требуется автоматический claim (для standalone-устройств без UI):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

После `requestClaim()` бэкенд выдаёт PIN, который пользователь должен ввести в портале.

### `seedSerialFromMac()` сгенерировал серийник, но в портале ввели другой

Серийник, сохранённый в NVS, имеет приоритет над MAC-генерацией. `seedSerialFromMac()` пишет в NVS только если серийника там ещё нет. Чтобы сменить серийник, очистите NVS:

```cpp
s_credentials.clear();
```

## MQTT

### Стейт-машина зашла в `MqttConnecting`, но не выходит в `Online`

Причины:

- Брокер недоступен. Production: `mqtt.idryer.org:8883`, staging: `staging.idryer.org:1884`.
- `MQTT_USE_TLS=1` без корректного корневого CA — handshake падает молча.
- `setBufferSize(16384)` не применяется — размер MQTT-буфера в `PubSubClient` по умолчанию 256 байт. `MqttClient` уже устанавливает 16384, но если вы используете `PubSubClient` напрямую — установите буфер сами.
- Persistent session "залипла" на брокере с другим client ID. Очистите NVS и перепрошейте.

### Команды от бэкенда не приходят

Проверьте подписку — `MqttClient` подписывается на `idryer/{serial}/commands/#` с QoS 1. Если подписка не удалась, в логе будет:

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

Проверьте, что `setCommandHandler()` вызван **до** `runtime.begin()` — иначе первая порция команд может пройти мимо.

### `PubSubClient` отключается с интервалом ровно 60 секунд

Это keep-alive timeout. Возможно, ваш MQTT-loop не вызывается достаточно часто — `s_runtime.loop()` должен крутиться без длинных блокировок. Проверьте, что в `loop()` нет `delay(>500ms)` и нет блокирующих сетевых вызовов.

## Команды и обработчики

### `commands/invoke` приходит, но `ActionDispatcher` не вызывается

Если вы зарегистрировали `setCommandHandler()`, **встроенный fallback на `ActionDispatcher` отключается**. `IdryerRuntime` отдаёт всё (кроме `ping`) в ваш `CommandHandler`. В нём нужно явно вызвать `s_dispatcher.handleInvoke(data)` для команд `invoke`.

Шаблон:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... продуктовые команды ...
}
```

### `commands/set` принят, но конфиг не применился

`ActionDispatcher::handleSet` извлекает `id` и `val` и передаёт в зарегистрированный `SetCallback`. Проверьте:

- `dispatcher.setSetCallback(onSetCommand, nullptr)` вызван в `setup()`.
- В `onSetCommand` действительно вызывается `s_profile.applyConfig(id, val)`.
- `applyConfig` возвращает `true` для известных `id`. Для неизвестных — возвращает `false`, изменения игнорируются.

## Telemetry

### Telemetry не публикуется

`idryer-core` не публикует телеметрию автоматически. Это всегда делает product code.

Проверьте, что:

- В `loop()` действительно вызывается `pub.publishTelemetry(doc)` (или `s_mqtt.publishTelemetry(doc)`, если LocalAccess не используется).
- Условие частоты не отрезает все вызовы. Типичная ошибка:
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  При первом запуске `lastTm == 0` и `millis()` ещё маленький — ветка не выполняется. Используйте `>=` и инициализацию `lastTm` в первом проходе.
- `s_runtime.isOnline() == true`. До Online MQTT отключён — публикация не пройдёт.
- Размер `JsonDocument` достаточен для payload. Проверьте `doc.overflowed()` после `serializeJson`.

### `publishTelemetry` возвращает `false`

Причины:

- Не подключён к брокеру (`MqttClient::isConnected() == false`).
- Превышен буфер — payload больше `MQTT_BUFFER_SIZE` (16384 байт). Для больших данных используйте `publishConfigRaw` (с чанками) или сократите payload.

### `DevicePublisher::publishTelemetry` не доходит до WS-клиента

`DevicePublisher` не возвращает ошибку, если WS-клиент не подключён — он просто пропускает WS-часть. Проверьте `s_local.isClientConnected()`. Если `false` — клиент не аутентифицирован или не подключён.

## NTP и время системы

### Время устройства не синхронизировано

NTP-синхронизация запускается в `setStateChangeCallback` после первого выхода из `WifiConnecting`:

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

Если этот колбэк не зарегистрирован — время не синхронизируется автоматически. TLS-handshake к брокеру требует валидного времени, иначе сертификат считается просроченным/из будущего.

Альтернативный канал: `IdryerRuntime` обрабатывает `commands/ping` и применяет `data["timestamp"]` через `settimeofday()`. Если бэкенд шлёт ping раз в минуту — время обновляется без NTP.

### TLS-handshake падает после долгого аптайма

Если NTP-сервер недоступен и устройство долго работает без перезагрузки, время может сбиться (особенно на ESP32-C3 без TCXO). Симптом: внезапный `connection failed` после нескольких суток uptime.

Решение: убедиться, что `pool.ntp.org` доступен из вашей сети, либо чаще получать `commands/ping` от бэкенда.

### `getIsoTimestamp` возвращает 1970 год

Время системы ещё не синхронизировано. Время появляется после первого успешного `configTime` или `commands/ping`. До этого момента `info`/`telemetry` будут публиковаться с заглушкой.

## ArduinoJson

### Compile error: `StaticJsonDocument` is not a member of `ArduinoJson`

Вы используете ArduinoJson v7. Тип `StaticJsonDocument` есть только в v6. Решения:

- Зафиксируйте v6 в `platformio.ini`:
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- Либо мигрируйте свой код на v7 API (`JsonDocument` вместо `StaticJsonDocument<N>`). `idryer-core` написан под v6.

### Compile error: ambiguous overload или несовпадение типов

В одном проекте могут оказаться две версии ArduinoJson через транзитивные зависимости. Проверьте:

```bash
pio pkg list -e my-device | grep -i arduinojson
```

Должна быть **одна** версия. Если две — закрепите явно через `lib_deps` и при необходимости через `lib_ldf_mode = chain+` или `lib_ignore`.

### `doc.overflowed()` true после serializeJson

Размер `StaticJsonDocument<N>` слишком мал для payload. Увеличьте `N` либо используйте `DynamicJsonDocument` для редко вызываемых путей.

## Local WS (LocalAccess)

### App не обнаруживает устройство в LAN

mDNS должен быть запущен **до** WiFi-подключения нет смысла — но **сразу после получения серийного номера** через `s_local.initMdns(serial)`. Проверьте:

- Маршрутизатор не блокирует multicast.
- App ищет `_idryer._tcp` на порту 81.
- Серийный номер устройства совпадает с тем, что зарегистрирован в портале.

### WS-клиент подключился, но получает `auth_required`

Первое сообщение от клиента должно быть `{"type":"auth","token":"<device_token>"}`. Если токен невалиден, `LocalAccess` вызывает `setTokenRefreshCallback()`. Продукт обязан в этом колбэке перечитать токен из `ICredentialStore` и вызвать `s_local.updateToken(...)`.

## Память и стабильность

### Free heap падает со временем

`PubSubClient::loop()` и `WebSocketsServer::loop()` не должны течь, но проверьте свой product code:

- `JsonDocument` создавайте на стеке (`StaticJsonDocument<N>`), не на куче (`DynamicJsonDocument`) для часто вызываемых путей.
- `String` в продуктовом коде на ESP32-C3 быстро фрагментирует кучу — используйте `char[]` и `snprintf`.

### `Stack overflow` или `Guru Meditation`

`s_runtime.loop()` не запускает FreeRTOS-задач — всё крутится в Arduino loop. Если падение в стек, ищите:

- Большие локальные `JsonDocument`/`char[8192]` на стеке Arduino loop (default 8 КБ).
- Глубокую рекурсию в продуктовом коде.

Увеличить стек Arduino loop:

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi (provisioning через Serial)

### Improv не принимает credentials

Improv должен владеть `Serial` до получения credentials:

```cpp
idryer::hal::initArduinoHal(nullptr);   // логи в /dev/null пока Improv держит Serial
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // вернуть лог обратно
}
```

Если `HAL_LOG_*` пишут в `Serial` параллельно с Improv-протоколом, Improv ломается на checksum.

### Improv-клиент не видит устройство

Проверьте `ChipFamily` в `setDeviceInfo`. Должен совпадать с реальным чипом: `CF_ESP32_C3`, `CF_ESP32_S3`, `CF_ESP32_S2`, `CF_ESP32`. Несовпадение — клиент Improv не покажет устройство в списке.

Также убедитесь, что baudrate Serial — 115200. Improv-протокол этого ожидает.

## Диагностика интеграций

### Полный диагностический выхлоп (1 Hz)

Меню → `DIAGNOSTICS → DIAG LOG` (`menu.diag_en`). По умолчанию выключено.
Включается через UI устройства, портал (`commands/set` с `bind=diag_en`),
либо REPL (`set diag_en 1`).

При включении раз в секунду в Serial выводится блок:

```
=========== iHeater Link diagnostics ===========
[device]    serial=DEVICE_... online=1 uptime=42s
[wifi]      status=3 ssid=Apart_4 ip=192.168.0.140 rssi=-51
[rmt-out]   mode=DRYING target=70.0°C
[active]    bambu
[bambu]     state=CONNECTED  ip=192.168.0.171 serial=<set> lan=<set>
            gcode_state='RUNNING' tray='PLA' chamber_target=0.0 chamber_temp=0.0
[moonraker] state=DISABLED   ws=ws://192.168.0.171:7125
            vc.available=0 vc.target=0.0 vc.temp=0.0 vc.has_sensor=0
[ha]        state=DISABLED   host=<empty>:1883 user=<empty>
[menu]      bambu_en=1 moon_en=0 ha_en=0 diag_en=1  mat_pla=45 ...
================================================
```

Полезно для удалённой диагностики: пользователь включает `DIAG LOG`, копирует
выхлоп → видно state коннекторов, lastError, что реально идёт на RMT.

### ANOMALY-канал (event-based)

Независимо от `diag_en` коннекторы и хелперы пишут отдельные строки с
префиксом `[!] ANOMALY` при неожиданных ситуациях:

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

Префикс `[!]` визуально выделяет аномалию в общем потоке логов. Это первое,
что нужно искать в Serial при «не работает».

### Auto-OFF при потере связи (fail-safe)

Если активная интеграция теряет соединение (TCP/WS disconnect), коннектор
немедленно сбрасывает целевую температуру:

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`, `available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF.
- **Bambu** — переход `Connected → !Connected` → `chamberTarget=0`, `trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF.
- **HA** — fail-safe пока не реализован.

Без этой логики нагрев продолжался бы на последнем известном target до
восстановления коннекта.

### Bambu: gcode_state-фильтр

`auto_heat` греет **только** при `gcode_state == "RUNNING"` или `"PREPARE"`.
Все остальные (`IDLE`, `FINISH`, `FAILED`, `PAUSE`, `INIT`, `OFFLINE`,
`SLICING`, `UNKNOWN`, пусто) → OFF.

При диагностике обращайте внимание на `gcode_state` в `[bambu]`-строке
diagnostics — если там `IDLE`/`FINISH`, нагрева не будет независимо от
наличия активного трея.

### Стенды для отладки без принтера

Для проверки интеграций без реальных принтеров продуктовые
репозитории (например, iHeater-link) могут содержать утилиты-заглушки
вроде `fake_moonraker` / `fake_bambu`, которые шлют лестницу значений
каждые 30 секунд.
