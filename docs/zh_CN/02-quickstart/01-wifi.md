# 步骤 01 — 使用 Improv 进行 WiFi 配置

完成此步骤后，您的 ESP32 将连接到 WiFi，凭证将保存到 NVS 以便在下一次重启时自动重新连接。门户和 MQTT 将在下一步中进行。

## 您需要什么

**硬件：**

- ESP32-C3 主板（DevKit、Super Mini 或兼容）
- USB 电缆（USB-C 或 Micro-USB，取决于您的主板）

**软件：**

- VS Code 中的 PlatformIO
- Chrome 或 Edge 浏览器（Safari 或 Firefox 不支持 Web Serial API）

## 步骤

**1. 创建 `platformio.ini`**，位于您项目的根目录：

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

将 `board` 替换为您主板的值（`esp32-c3-devkitm-1`、`seeed_xiao_esp32c3` 等）。

**2. 复制示例。** 取得 [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) 的内容并将其保存为您项目中的 `src/main.cpp`。

**3. 设置 ChipFamily。** 在复制的文件中，找到这一行：

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

确保 ChipFamily 与您的芯片匹配：`CF_ESP32_C3`、`CF_ESP32_S3` 或 `CF_ESP32`。

**4. 刷新：**

```bash
pio run -e improv-demo -t upload
```

**5. 在 Chrome 或 Edge 中打开 [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)**。点击**连接**并从浏览器对话框中选择设备 USB 端口。

**6. 为您的 2.4 GHz 网络输入 SSID 和密码**。网页将通过 Serial-Improv 向主板发送凭证。主板将将其保存到 NVS。

## 验证

打开串行监视器：

```bash
pio device monitor -b 115200
```

成功连接后您将看到：

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

如果此行未出现，请查看下面的故障排除链接。

!!! note
    如果凭证已从之前的运行保存在 NVS 中，主板将在启动时自动连接到 WiFi — 不需要 Improv。

## 下一步

- [02-claim.md](02-claim.md) — 将设备绑定到您的 idryer.org 帐户。
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — 如果 WiFi 无法连接。
