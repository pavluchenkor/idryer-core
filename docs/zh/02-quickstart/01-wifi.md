# 步驟 01 — 使用 Improv 進行 WiFi 佈建

完成此步驟後，您的 ESP32 將連接到 WiFi，認證將保存到 NVS 以便在下一次重啟時自動重新連接。門戶和 MQTT 將在下一步中進行。

## 您需要什麼

**硬件：**

- ESP32-C3 主機板（DevKit、Super Mini 或相容）
- USB 線纜（USB-C 或 Micro-USB，取決於您的主機板）

**軟件：**

- VS Code 中的 PlatformIO
- Chrome 或 Edge 瀏覽器（Safari 或 Firefox 不支持 Web Serial API）

## 步驟

**1. 建立 `platformio.ini`**，位於您項目的根目錄：

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

將 `board` 替換為您主機板的值（`esp32-c3-devkitm-1`、`seeed_xiao_esp32c3` 等）。

**2. 複製示例。** 取得 [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) 的內容並將其保存為您項目中的 `src/main.cpp`。

**3. 設置 ChipFamily。** 在複製的文件中，找到這一行：

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

確保 ChipFamily 與您的芯片匹配：`CF_ESP32_C3`、`CF_ESP32_S3` 或 `CF_ESP32`。

**4. 刷新：**

```bash
pio run -e improv-demo -t upload
```

**5. 在 Chrome 或 Edge 中打開 [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)**。點擊**連接**並從瀏覽器對話框中選擇設備 USB 端口。

**6. 為您的 2.4 GHz 網絡輸入 SSID 和密碼**。網頁將通過 Serial-Improv 向主機板發送認證。主機板將將其保存到 NVS。

## 驗證

打開串行監視器：

```bash
pio device monitor -b 115200
```

成功連接後您將看到：

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

如果此行未出現，請查看下面的故障排除鏈接。

!!! note
    如果認證已從之前的運行保存在 NVS 中，主機板將在啟動時自動連接到 WiFi — 不需要 Improv。

## 下一步

- [02-claim.md](02-claim.md) — 將設備綁定到您的 idryer.org 帳戶。
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — 如果 WiFi 無法連接。
