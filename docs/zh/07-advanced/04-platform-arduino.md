# Arduino 平台

库定义了三个接口来抽象平台：

- `IWifiManager` — WiFi 管理。
- `ICredentialStore` — 设备身份存储。
- `IHttpClient` — HTTP 请求。

这些接口的 Arduino 实现位于 `platform/arduino/`。它们只为 ESP32/Arduino 编译。

## ArduinoWifiManager

在 Arduino `WiFi` 之上实现 `IWifiManager`。

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

`begin()` 保存凭据并启动连接。可以安全地多次调用（例如 Improv provisioning 之后）。

`loop()` 在 `CloudStateMachine::loop()` 内部调用。产品不需要调用它。

## ArduinoCredentialStore

通过 ESP32 NVS（`Preferences`）实现 `ICredentialStore`，namespace 为 `"idryer"`。

存储三个字段：

| NVS key | 内容 |
|---------|---------|
| `serial` | 设备序列号（MQTT 用户名） |
| `token` | 设备 token（MQTT 密码） |
| `deviceId` | 后端 UUID（claim 后） |

```cpp
bool load(DeviceIdentity& identity);  // true if token is not empty
bool save(const DeviceIdentity& identity);
void clear();
```

额外方法：

```cpp
void seedSerialFromMac();
```

如果 NVS 中没有序列号，会根据 WiFi MAC 地址生成 `DEVICE_AABBCCDDEEFF` 格式的序列号并保存。请在 `setup()` 中、`runtime.begin()` 之前调用。

## ArduinoHttpClient

通过 `WiFiClientSecure` 实现 `IHttpClient`。

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // default 10000 ms
```

使用 Let's Encrypt ISRG Root X1 根 CA（来自 `root_ca.h`）。`CloudStateMachine` 用它进行 provisioning 和 claim 轮询。产品不会直接调用它。

## ArduinoWifiStore

独立类（不实现接口），用于在 NVS 的 `"wifi"` namespace 中存储 WiFi 凭据。与 Improv WiFi 一起使用。

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

`setup()` 中的典型用法：

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

## HAL: ArduinoTime 和 ArduinoLogger

`hal/hal_arduino.h` 包含 HAL 接口的 Arduino 实现：

- `ArduinoTime` — 委托 `millis()`、`micros()`、`delay()`、`delayMicroseconds()`。
- `ArduinoLogger` — 按级别和 ANSI 颜色向 `Stream` 格式化输出。
- `ArduinoSerial` — 为 `UartBridge` 封装 `HardwareSerial`。

初始化：

```cpp
// In setup() — logs disabled while Improv owns Serial
idryer::hal::initArduinoHal(nullptr);

// After WiFi connects
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` 可以安全调用：所有 `HAL_LOG_*` 宏都会变成 no-op。

## 为什么需要此抽象

`CloudStateMachine` 接收 `IWifiManager*` 和 `ICredentialStore*`。这允许：

- 在没有真实 WiFi 的主机上运行测试（替换为 mock）。
- 支持另一个平台（非 Arduino），而不修改库核心。
- 独立于硬件测试 provisioning 逻辑。

实际中，iDryer 产品只使用 Arduino 实现。
