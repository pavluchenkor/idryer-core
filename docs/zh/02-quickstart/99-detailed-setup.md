# 详细设置

捷径是 [5 分钟运行](01-five-minutes.md)。本页介绍环境、编译标志、日志和开发模式。

## 项目中的核心

核心位于 PlatformIO 项目的 `lib/idryer-core`：可以是副本、git 子模块，或指向共享克隆的符号链接。它的 `library.json` 会带来依赖：MQTT、ArduinoJson、WebSockets、Improv。项目的 `lib_deps` 只写你的传感器库。

基于核心的产品所用的板子：ESP32-C3（DevKit、Super Mini）、ESP32-S3（XIAO ESP32-S3、Waveshare ESP32-S3 Zero）。

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP 是 espMqttClient 依赖中的 ESP8266 传输层，在 ESP32 上无法编译。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; 没有 USB-UART 的板子（ESP32-C3 SuperMini）：Serial 走 USB。
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| 标志 | 用途 |
|---|---|
| `IDRYER_API_BASE` | 门户 API 地址：激活、绑定 |
| `MQTT_BROKER`、`MQTT_PORT` | 门户的 broker |
| `MQTT_USE_TLS=1` | 与 broker 的安全连接 |
| `lib_ignore = ESPAsyncTCP` | MQTT 客户端依赖中的 ESP8266 传输层：在 ESP32 上无法编译 |
| `ARDUINO_USB_MODE`、`ARDUINO_USB_CDC_ON_BOOT` | 没有 USB-UART 的板子让 Serial 走 USB |

字符串宏内外都需要引号。

## 发布周期

`Config.telemetryPeriodMs`、`telemetryPeriodIdleMs`、`statusPeriodMs`、`statusPeriodIdleMs` 字段；0 表示契约中的值：

| 内容 | 工作时 | 空闲时 |
|---|---|---|
| 遥测 | 30 秒 | 60 秒 |
| 状态 | 模式、设定值或时间变化时立即发布；每 60 秒核对一次 | 每 5 分钟核对一次 |

“空闲”指没有任何单元处于活动模式。

## 日志

```bash
pio device monitor -b 115200
```

设备没有网络时日志保持安静：端口被 Improv 占用。出现 `[BOOT] WiFi ok, logs enabled` 这一行后日志开启。此后 `STATUS`、`WIPE_IDENTITY`、`PAIR_TOKEN` 命令可用：见 [绑定到账户](02-claim.md)。

## 开发模式：`IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

加上这个标志后：

- 通电后日志立即输出到端口；
- Improv 以及 `STATUS`、`WIPE_IDENTITY`、`PAIR_TOKEN` 命令关闭：端口收到的行由你的代码读取；
- 网络由应用（ESPTouch）或代码提供：在 `begin()` 之前调用 `seedWifiCredentialsIfEmpty()`，见 [Wi-Fi](01-wifi.md)。

发布的固件不带这个标志编译。

## 下一步

- [核心示例](https://github.com/pavluchenkor/idryer-core/tree/main/examples)。
- [如何添加新产品](../09-add-product/01-add-new-product.md)。
- [设备卡片：card 清单](../09-add-product/02-add-widget.md)。
