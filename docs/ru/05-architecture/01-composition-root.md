# Composition Root

Продукт создаёт все объекты библиотеки в `main.cpp` как статические переменные и передаёт зависимости через конструкторы. Нет фабрик, нет глобального реестра — только явная сборка.

## Порядок создания объектов

Зависимости строятся снизу вверх: сначала платформенный слой, потом облачный стек, потом рантайм.

```cpp
// 1. Платформенный слой
idryer::ArduinoWifiStore       s_wifiStore;      // NVS: SSID/password
idryer::ArduinoWifiManager     s_wifi;           // управление WiFi
idryer::ArduinoCredentialStore s_credentials;    // NVS: serial/token/deviceId
idryer::ArduinoHttpClient      s_http;           // TLS HTTP для provisioning

// 2. Облачный стек TODO: описание назначения 
idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
idryer::MqttClient               s_mqtt;
idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
idryer::ActionDispatcher         s_dispatcher;

// 3. Профиль продукта (реализует IProfile) — product code, не библиотека
LedStripProfile s_profile(&s_executor);

// 4. Рантайм — связывает всё
idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
```

## Что делает setup()

```cpp
void setup() {
    // HAL: логи идут в /dev/null пока Improv владеет Serial
    idryer::hal::initArduinoHal(nullptr);

    // Восстановить сохранённые WiFi-credentials
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    }

    // Сгенерировать serial из MAC если ещё нет
    s_credentials.seedSerialFromMac();

    // Зарегистрировать обработчики команд
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    // Опционально: реакция на переходы стейт-машины
    s_cloud.setStateChangeCallback([](auto prev, auto, void*) {
        if (prev == idryer::cloud::CloudState::WifiConnecting)
            configTime(0, 0, "pool.ntp.org", "time.google.com");
    }, nullptr);

    // Автоматический claiming для standalone-устройств
    s_cloud.setUnclaimedCallback([](void*) {
        s_cloud.requestClaim();
    }, nullptr);

    // Запустить рантайм
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();     // CloudStateMachine + IProfile::loop()
    // ... продуктовая логика (сенсоры, телеметрия)
}
```

## Правила сборки

- Все объекты библиотеки — статические (`static`). Никаких `new` и `malloc` для объектов верхнего уровня.
- `runtime.begin()` вызывается последним в `setup()`, после того как все обработчики зарегистрированы.
- `runtime.loop()` вызывается первым в `loop()`.
- Продуктовые объекты (сенсоры, телеметрия) создаются отдельно и подключаются к `s_mqtt` напрямую — рантайм о них не знает.

## Пример: Storage Link

Полный composition root Storage Link: [`src/main.cpp`](../../../../src/main.cpp).

Слои устройства в порядке сборки:

| Слой | Объекты | Источник |
|------|---------|----------|
| Платформа | `s_wifiStore`, `s_wifi`, `s_credentials`, `s_http` | `idryer-core` |
| Облако | `s_api`, `s_mqtt`, `s_cloud`, `s_dispatcher` | `idryer-core` |
| Устройство | `s_executor`, `s_profile` | `src/storage/led_strip/` |
| Рантайм | `s_runtime` | `idryer-core` |
| Сенсоры | `s_sensor`, `s_telemetry` | `src/storage/sensors/`, `src/storage/telemetry/` |
