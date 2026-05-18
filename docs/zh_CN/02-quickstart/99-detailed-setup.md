# 详细设置

如果这是您第一次来到这里——请访问 [5 分钟入门](01-five-minutes.md)；本页涵盖高级设置和故障排除。

简短路径：连接库、刷写示例、查看闪烁的 LED 和门户中的设备。

## 需要准备什么

- ESP32 板（推荐：ESP32-C3 DevKit、Super Mini、XIAO ESP32-S3、Waveshare ESP32-S3 Zero）。
- PlatformIO，框架为 `arduino`，平台为 `espressif32`。
- WiFi 2.4 GHz 和互联网访问。
- [portal.idryer.org](https://portal.idryer.org/) 上的帐户用于声称设备。

## 步骤 1. 连接库

在您产品的 `platformio.ini` 中：

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; 仅对 mqtt_with_local_ws 需要

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

## 步骤 2. 创建 `secrets.h`

将 [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) 复制到项目中的 `include/secrets.h` 并填入您的 SSID/密码。该文件必须在 `.gitignore` 中。

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

`IDRYER_API_BASE` 通常通过 `build_flags` 设置，而不是通过 secrets.h 设置。

## 步骤 3. 打开第一个示例

最简单的是 [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino)。将其作为您的起点复制：

- 不需要传感器、外围设备或 LAN WS。
- 不需要手动 `handleCommand` — `IdryerRuntime` 中的内置回退处理基本命令。
- LED 在设备在线时闪烁——这是成功的指标。

## 步骤 4. 刷写并观察

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

预期的日志序列：

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← 等待声称
[CSM] PIN: 1234567   expires in 600s          ← 如果启用自动声称
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← 就绪，LED 开始闪烁
[RT]  Cloud Online
```

## 步骤 5. 声称设备到您的帐户

自动声称已在示例中启用。PIN 显示在日志中。在 [portal.idryer.org](https://portal.idryer.org/) → "添加设备" 中输入它。声称后，`CloudStateMachine` 转换到 `Online`。

## 接下来做什么

以下示例各引入一个新的复杂程度：

| 示例 | 添加的内容 |
|------|----------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | 自定义 `handleCommand`，处理 `commands/invoke` 和 `commands/set` |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | 通过 Improv 进行 WiFi 配置（无硬编码凭据） |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | 本地 LAN WebSocket 服务器 + `DevicePublisher`（一次发布——两个传输） |

## 通过串行的开发 REPL（无门户、无浏览器）

开发者的替代路径——在标准串行监视器中直接查看完整的声称流，无需 Improv 和门户 UI。

在 `platformio.ini` 中，使用标志 `-DIDRYER_DEV_REPL=1` 创建开发环境：

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

标志启用的功能：
- HAL 日志到 `Serial` **立即**从启动开始（在 WiFi 连接前无沉默）。
- Improv 配置被**禁用**——串行空闲用于交互命令。
- 简单的 REPL 出现在 `main.cpp` 中：`wifi`、`claim`、`status`、`wipe`、`restart`、`help`。

完整流程：

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

在监视器中：

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — 在门户中输入
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

REPL 接受命令无论串行监视器中的行尾设置如何（`\n`、`\r` 或空闲超时 120 ms）——在任何终端中工作，包括 `pio device monitor`、Arduino IDE 串行监视器、`screen`、`picocom`。

生产构建（`-e my-device-prod`，不带 `IDRYER_DEV_REPL`）通过 Chrome 使用 Improv（`https://www.improv-wifi.com/`）且不包含 REPL 代码——标志是编译时的，节省 Flash。

带有 `WIFI_SSID/WIFI_PASSWORD` 的 `secrets.h`（步骤 2）仍然是无头 CI/自动刷写场景的单独路径——在两个环境中都有效。

任何示例启动并运行后，请阅读：

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — `main.cpp` 中的对象顺序。
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — 数据如何移动。
- [04-patterns/](../04-patterns/) — 配方：添加传感器、外围设备、传输。
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — 新产品的完整检查清单。
- [10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — 如果堆栈卡住了怎么办。
