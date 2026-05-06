# Arduino-платформа

Библиотека определяет три интерфейса для абстрагирования платформы:

- `IWifiManager` — управление WiFi.
- `ICredentialStore` — хранение device identity.
- `IHttpClient` — HTTP-запросы.

Arduino-реализации этих интерфейсов находятся в `platform/arduino/`. Они компилируются только для ESP32/Arduino.

## ArduinoWifiManager

Реализует `IWifiManager` поверх Arduino `WiFi`.

```cpp
class ArduinoWifiManager : public IWifiManager {
    void begin(const char* ssid, const char* password) override;
    bool connect() override;
    bool isConnected() override;
    void disconnect() override;
    void getLocalIP(char* buffer, size_t bufferSize) override;
    void getSSID(char* buffer, size_t bufferSize) override;
    int  getRSSI() override;
    void getMacAddress(char* buffer, size_t bufferSize) override;
    void loop() override;
};
```

`begin()` сохраняет credentials и инициирует подключение. Безопасно вызывать повторно (например, после Improv provisioning).

`loop()` вызывается внутри `CloudStateMachine::loop()`. Продукту вызывать не нужно.

## ArduinoCredentialStore

Реализует `ICredentialStore` через ESP32 NVS (`Preferences`), namespace `"idryer"`.

Хранит три поля:

| NVS-ключ | Содержание |
|----------|-----------|
| `serial` | серийный номер устройства (MQTT username) |
| `token` | токен устройства (MQTT password) |
| `deviceId` | backend UUID (после claiming) |

```cpp
bool load(DeviceIdentity& identity);  // true если token не пустой
bool save(const DeviceIdentity& identity);
void clear();
```

Дополнительный метод:

```cpp
void seedSerialFromMac();
```

Если в NVS нет серийника — генерирует его из MAC-адреса WiFi в формате `DEVICE_AABBCCDDEEFF` и сохраняет. Вызвать в `setup()` до `runtime.begin()`.

## ArduinoHttpClient

Реализует `IHttpClient` через `WiFiClientSecure`.

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // по умолчанию 10000 мс
```

Использует корневой CA Let's Encrypt ISRG Root X1 (из `root_ca.h`). Применяется `CloudStateMachine` для provisioning и claim-polling. Продукт его не вызывает напрямую.

## ArduinoWifiStore

Отдельный класс (не реализует интерфейс) для хранения WiFi credentials в NVS, namespace `"wifi"`. Используется совместно с Improv WiFi.

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

Типичное использование в `setup()`:

```cpp
ArduinoWifiStore wifiStore;

// Восстановить сохранённые credentials
char ssid[64], pass[64];
if (wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
    wifi.begin(ssid, pass);
}

// Сохранить после Improv
improv.onImprovConnected([&](const char* s, const char* p) {
    wifiStore.save(s, p);
    wifi.begin(s, p);
});
```

## HAL: ArduinoTime и ArduinoLogger

`hal/hal_arduino.h` содержит Arduino-реализации HAL-интерфейсов:

- `ArduinoTime` — делегирует `millis()`, `micros()`, `delay()`, `delayMicroseconds()`.
- `ArduinoLogger` — форматированный вывод в `Stream` с уровнями и ANSI-цветами.
- `ArduinoSerial` — оборачивает `HardwareSerial` для `UartBridge`.

Инициализация:

```cpp
// В setup() — логи выключены пока Improv владеет Serial
idryer::hal::initArduinoHal(nullptr);

// После подключения WiFi
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` безопасна для вызова: все `HAL_LOG_*` макросы становятся no-op.

## Почему нужна эта абстракция

`CloudStateMachine` принимает `IWifiManager*` и `ICredentialStore*`. Это позволяет:

- Запускать тесты на хосте без реального WiFi (подменить моками).
- Поддержать другую платформу (не Arduino) без изменения ядра библиотеки.
- Тестировать логику provisioning независимо от железа.

На практике в продуктах iDryer используются только Arduino-реализации.
