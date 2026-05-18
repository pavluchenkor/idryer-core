# 5分钟快速开始

完成此页面后，您的ESP32将被刷入，将连接到WiFi，并在[portal.idryer.org](https://portal.idryer.org/)上显示为在线状态。要求：ESP32-C3（DevKit、Super Mini 或兼容）、USB 电缆、VS Code 中的 PlatformIO。

## 1. 准备 secrets.h

将[`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example)复制到您项目的`include/secrets.h`并设置您的WiFi SSID和密码（仅2.4 GHz）：

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

将`include/secrets.h`添加到`.gitignore`。

## 2. 配置 platformio.ini

在项目根目录中创建`platformio.ini`：

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

将`board`更改为匹配您的主板。将`path/to/idryer-core`替换为库的实际路径。

## 3. 复制01_blink_status示例

将[`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino)的内容复制到您项目的`src/main.cpp`。此示例不需要传感器或其他依赖项 — 只需要一个最小的组合根。

## 4. 刷入

```bash
pio run -e blink-demo -t upload
```

## 5. 打开串行监视器

```bash
pio device monitor -b 115200
```

预期日志序列：

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

在门户中输入PIN（步骤6）后：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

如果设备在`PIN: ...`消息处停止 — 这是正常的；继续执行步骤6。

## 6. 在门户中声称设备

打开[portal.idryer.org](https://portal.idryer.org/)，转到**添加设备**，然后输入串行监视器中的PIN。成功声称后，设备将转变为`在线`，内置LED将每500毫秒闪烁一次。

详细的声称流程：[登录](02-onboarding.md)。

## 接下来做什么

- 添加传感器 — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- 添加外围设备 — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- 完整API参考 — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- 内部工作原理 — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
