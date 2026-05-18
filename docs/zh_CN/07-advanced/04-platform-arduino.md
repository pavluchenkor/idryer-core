# Arduino 平台

库定义了三个接口来抽象平台：

- `IWifiManager` — WiFi 管理。
- `ICredentialStore` — 设备身份存储。
- `IHttpClient` — HTTP 请求。

这些接口的 Arduino 实现在 `platform/arduino/` 中。它们仅为 ESP32/Arduino 编译。

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

`begin()` 存储凭据并启动连接。可以安全地调用多次（例如，在 Improv 配置之后）。

`loop()` 在 `CloudStateMachine::loop()` 内调用。产品不需要调用它。

## ArduinoCredentialStore

通过 ESP32 NVS（`Preferences`）、命名空间 `"idryer"` 实现 `ICredentialStore`。

存储三个字段：

| NVS 键 | 内容 |
|--------|------|
| `serial` | 设备序列号（MQTT 用户名） |
| `token` | 设备令牌（MQTT 密码） |
| `deviceId` | 后端 UUID（声称后） |

```cpp
bool load(DeviceIdentity& identity);  // 如果令牌不为空，则为 true
bool save(const DeviceIdentity& identity);
void clear();
```

其他方法：

```cpp
void seedSerialFromMac();
```

如果 NVS 没有序列号——从 WiFi MAC 地址生成一个，格式为 `DEVICE_AABBCCDDEEFF`，并保存它。在 `setup()` 中的 `runtime.begin()` 之前调用。

## ArduinoHttpClient

通过 `WiFiClientSecure` 实现 `IHttpClient`。

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // 默认 10000 毫秒
```

使用 Let's Encrypt ISRG Root X1 根 CA（来自 `root_ca.h`）。由 `CloudStateMachine` 用于配置和声称轮询。产品不直接调用它。

## ArduinoWifiStore

单独的类（不实现接口）用于在 NVS 中存储 WiFi 凭据，命名空间 `"wifi"`。与 Improv WiFi 一起使用。

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

在 `setup()` 中的典型用法：

```cpp
ArduinoWifiStore wifiStore;

// 恢复保存的凭据
char ssid[64], pass[64];
if (wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
    wifi.begin(ssid, pass);
}

// 在 Improv 之后保存
improv.onImprovConnected([&](const char* s, const char* p) {
    wifiStore.save(s, p);
    wifi.begin(s, p);
});
```

## HAL：ArduinoTime 和 ArduinoLogger

`hal/hal_arduino.h` 包含 HAL 接口的 Arduino 实现：

- `ArduinoTime` — 委托 `millis()`、`micros()`、`delay()`、`delayMicroseconds()`。
- `ArduinoLogger` — 格式化输出到 `Stream`，带有级别和 ANSI 颜色。
- `ArduinoSerial` — 为 `UartBridge` 包装 `HardwareSerial`。

初始化：

```cpp
// 在 setup() 中 — 当 Improv 拥有 Serial 时日志被禁用
idryer::hal::initArduinoHal(nullptr);

// WiFi 连接后
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` 是安全调用的：所有 `HAL_LOG_*` 宏变成无操作。

## 为什么需要这个抽象

`CloudStateMachine` 接受 `IWifiManager*` 和 `ICredentialStore*`。这允许：

- 在没有真实 WiFi 的主机上运行测试（替换为模拟）。
- 支持另一个平台（非 Arduino）而不改变库核心。
- 独立于硬件测试配置逻辑。

在实践中，只有 Arduino 实现在 iDryer 产品中使用。
