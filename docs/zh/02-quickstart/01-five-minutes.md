# 5分鐘快速開始

完成此頁面後，您的ESP32將被刷入，將連接到WiFi，並在[portal.idryer.org](https://portal.idryer.org/)上顯示為線上狀態。需求：ESP32-C3（DevKit、Super Mini 或相容）、USB線纜、VS Code中的PlatformIO。

## 1. 準備 secrets.h

將[`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example)複製到您項目的`include/secrets.h`並設置您的WiFi SSID和密碼（僅2.4 GHz）：

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

將`include/secrets.h`添加到`.gitignore`。

## 2. 配置 platformio.ini

在項目根目錄中創建`platformio.ini`：

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

將`board`更改為匹配您的主機板。將`path/to/idryer-core`替換為庫的實際路徑。

## 3. 複製01_blink_status示例

將[`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino)的內容複製到您項目的`src/main.cpp`。此示例不需要傳感器或其他依賴項 — 只需要一個最小的組合根。

## 4. 刷入

```bash
pio run -e blink-demo -t upload
```

## 5. 打開串行監視器

```bash
pio device monitor -b 115200
```

預期日誌序列：

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

在門戶中輸入PIN（步驟6）後：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

如果設備在`PIN: ...`消息處停止 — 這是正常的；繼續執行步驟6。

## 6. 在門戶中聲稱設備

打開[portal.idryer.org](https://portal.idryer.org/)，轉到**添加設備**，然後輸入串行監視器中的PIN。成功聲稱後，設備將轉變為`線上`，內置LED將每500毫秒閃爍一次。

詳細的聲稱流程：[登錄](02-onboarding.md)。

## 接下來做什麼

- 添加傳感器 — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- 添加外圍設備 — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- 完整API參考 — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- 內部工作原理 — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
