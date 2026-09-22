# 詳細設定

捷徑是 [5 分鐘執行](01-five-minutes.md)。本頁介紹環境、編譯旗標、日誌和開發模式。

## 專案中的核心

核心位於 PlatformIO 專案的 `lib/idryer-core`：可以是副本、git 子模組，或指向共用複本的符號連結。它的 `library.json` 會帶入相依套件：MQTT、ArduinoJson、WebSockets、Improv。專案的 `lib_deps` 只寫你的感測器函式庫。

基於核心的產品所用的板子：ESP32-C3（DevKit、Super Mini）、ESP32-S3（XIAO ESP32-S3、Waveshare ESP32-S3 Zero）。

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP 是 espMqttClient 相依套件中的 ESP8266 傳輸層，在 ESP32 上無法編譯。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; 沒有 USB-UART 的板子（ESP32-C3 SuperMini）：Serial 走 USB。
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| 旗標 | 用途 |
|---|---|
| `IDRYER_API_BASE` | 門戶 API 位址：啟用、綁定 |
| `MQTT_BROKER`、`MQTT_PORT` | 門戶的 broker |
| `MQTT_USE_TLS=1` | 與 broker 的安全連線 |
| `lib_ignore = ESPAsyncTCP` | MQTT 用戶端相依套件中的 ESP8266 傳輸層：在 ESP32 上無法編譯 |
| `ARDUINO_USB_MODE`、`ARDUINO_USB_CDC_ON_BOOT` | 沒有 USB-UART 的板子讓 Serial 走 USB |

字串巨集內外都需要引號。

## 發布週期

`Config.telemetryPeriodMs`、`telemetryPeriodIdleMs`、`statusPeriodMs`、`statusPeriodIdleMs` 欄位；0 表示契約中的值：

| 內容 | 工作時 | 閒置時 |
|---|---|---|
| 遙測 | 30 秒 | 60 秒 |
| 狀態 | 模式、設定值或時間變化時立即發布；每 60 秒核對一次 | 每 5 分鐘核對一次 |

「閒置」指沒有任何單元處於活動模式。

## 日誌

```bash
pio device monitor -b 115200
```

裝置沒有網路時日誌保持安靜：連接埠被 Improv 占用。出現 `[BOOT] WiFi ok, logs enabled` 這一行後日誌開啟。此後 `STATUS`、`WIPE_IDENTITY`、`PAIR_TOKEN` 命令可用：見 [綁定到帳戶](02-claim.md)。

## 開發模式：`IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

加上這個旗標後：

- 通電後日誌立即輸出到連接埠；
- Improv 以及 `STATUS`、`WIPE_IDENTITY`、`PAIR_TOKEN` 命令關閉：連接埠收到的行由你的程式碼讀取；
- 網路由應用程式（ESPTouch）或程式碼提供：在 `begin()` 之前呼叫 `seedWifiCredentialsIfEmpty()`，見 [Wi-Fi](01-wifi.md)。

發布的韌體不帶這個旗標編譯。

## 下一步

- [核心範例](https://github.com/pavluchenkor/idryer-core/tree/main/examples)。
- [如何新增產品](../09-add-product/01-add-new-product.md)。
- [裝置卡片：card 清單](../09-add-product/02-add-widget.md)。
