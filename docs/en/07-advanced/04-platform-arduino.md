# Arduino platform

The library defines three interfaces to abstract the platform:

- `IWifiManager` — WiFi management.
- `ICredentialStore` — device identity storage.
- `IHttpClient` — HTTP requests.

Arduino implementations of these interfaces are in `platform/arduino/`. They are compiled only for ESP32/Arduino.

## ArduinoWifiManager

Implements `IWifiManager` on top of Arduino `WiFi`.

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

`begin()` stores credentials and initiates connection. Safe to call multiple times (e.g., after Improv provisioning).

`loop()` is called inside `CloudStateMachine::loop()`. The product does not need to call it.

## ArduinoCredentialStore

Implements `ICredentialStore` via ESP32 NVS (`Preferences`), namespace `"idryer"`.

Stores three fields:

| NVS key | Content |
|---------|---------|
| `serial` | device serial number (MQTT username) |
| `token` | device token (MQTT password) |
| `deviceId` | backend UUID (after claiming) |

```cpp
bool load(DeviceIdentity& identity);  // true if token is not empty
bool save(const DeviceIdentity& identity);
void clear();
```

Additional method:

```cpp
void seedSerialFromMac();
```

If NVS has no serial number — generates one from the WiFi MAC address in the format `DEVICE_AABBCCDDEEFF` and saves it. Call in `setup()` before `runtime.begin()`.

## ArduinoHttpClient

Implements `IHttpClient` via `WiFiClientSecure`.

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // default 10000 ms
```

Uses the Let's Encrypt ISRG Root X1 root CA (from `root_ca.h`). Used by `CloudStateMachine` for provisioning and claim polling. The product does not call it directly.

## ArduinoWifiStore

Separate class (does not implement an interface) for storing WiFi credentials in NVS, namespace `"wifi"`. Used together with Improv WiFi.

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

Typical usage in `setup()`:

```cpp
ArduinoWifiStore wifiStore;

// Restore saved credentials
char ssid[64], pass[64];
if (wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
    wifi.begin(ssid, pass);
}

// Save after Improv
improv.onImprovConnected([&](const char* s, const char* p) {
    wifiStore.save(s, p);
    wifi.begin(s, p);
});
```

## HAL: ArduinoTime and ArduinoLogger

`hal/hal_arduino.h` contains Arduino implementations of HAL interfaces:

- `ArduinoTime` — delegates `millis()`, `micros()`, `delay()`, `delayMicroseconds()`.
- `ArduinoLogger` — formatted output to `Stream` with levels and ANSI colors.
- `ArduinoSerial` — wraps `HardwareSerial` for `UartBridge`.

Initialization:

```cpp
// In setup() — logs disabled while Improv owns Serial
idryer::hal::initArduinoHal(nullptr);

// After WiFi connects
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` is safe to call: all `HAL_LOG_*` macros become no-ops.

## Why this abstraction is needed

`CloudStateMachine` accepts `IWifiManager*` and `ICredentialStore*`. This allows:

- Running tests on a host without real WiFi (replace with mocks).
- Supporting another platform (non-Arduino) without changing the library core.
- Testing provisioning logic independently of hardware.

In practice, only Arduino implementations are used in iDryer products.
