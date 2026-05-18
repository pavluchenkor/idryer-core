# 詳細設置

如果這是您第一次來 — 轉到 [5 分鐘快速入門](01-five-minutes.md)；本頁面涵蓋高級設置和故障排除。

簡短路徑：連接庫、刷寫示例、查看閃爍的 LED 和入口網站中的設備。

## 需要準備的內容

- ESP32 開發板（推薦：ESP32-C3 DevKit、Super Mini、XIAO ESP32-S3、Waveshare ESP32-S3 Zero）。
- PlatformIO，框架為 `arduino`，平台為 `espressif32`。
- 2.4 GHz WiFi 且具有互聯網訪問。
- [portal.idryer.org](https://portal.idryer.org/) 帳戶用於聲明設備。

## 第 1 步。連接庫

在您產品的 `platformio.ini` 中：

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; 僅在 mqtt_with_local_ws 時需要

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

## 第 2 步。創建 `secrets.h`

複製 [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) 到您項目中的 `include/secrets.h` 並填入您的 SSID/密碼。該文件必須在 `.gitignore` 中。

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

`IDRYER_API_BASE` 通常通過 `build_flags` 設置，而不是通過 secrets.h。

## 第 3 步。打開第一個示例

最簡單的是 [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino)。將其複製為您的起點：

- 不需要傳感器、外設或 LAN WS。
- 不需要手動 `handleCommand` — `IdryerRuntime` 中的內置回退處理基本命令。
- LED 在設備在線時閃爍 — 這是成功指示。

## 第 4 步。刷寫並觀察

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

預期的日誌序列：

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← 等待聲明
[CSM] PIN: 1234567   expires in 600s          ← 如果啟用自動聲明
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← 準備就緒，LED 開始閃爍
[RT]  Cloud Online
```

## 第 5 步。將設備聲明到您的帳戶

示例中已啟用自動聲明。PIN 出現在日誌中。在 [portal.idryer.org](https://portal.idryer.org/) → "添加設備" 中輸入它。聲明後，`CloudStateMachine` 轉變為 `Online`。

## 接下來要做什麼

以下示例各引入一個新的複雜程度級別：

| 示例 | 添加了什麼 |
|---------|--------------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | 自定義 `handleCommand`，處理 `commands/invoke` 和 `commands/set` |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | 通過 Improv 的 WiFi 配置（無硬編碼憑據） |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | 本地 LAN WebSocket 服務器 + `DevicePublisher`（一個發佈 — 兩個傳輸） |

## 通過串行的開發 REPL（無入口網站，無瀏覽器）

一個替代開發者路徑 — 在標準串行監視器中直接查看完整的聲明流，無需 Improv 和無需入口網站 UI。

在 `platformio.ini` 中，使用標誌 `-DIDRYER_DEV_REPL=1` 創建開發環境：

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

該標誌啟用的內容：
- HAL 日誌到 `Serial` 在啟動時**立即**開始（直到 WiFi 連接前無沉默）。
- Improv 配置**已禁用** — Serial 可用於交互式命令。
- 一個簡單的 REPL 出現在 `main.cpp` 中：`wifi`、`claim`、`status`、`wipe`、`restart`、`help`。

完整流程：

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

在監視器中：

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — 在入口網站中輸入
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

REPL 接受命令，無論串行監視器中的行結束設置如何（`\n`、`\r` 或空閒超時 120 ms）— 適用於任何終端，包括 `pio device monitor`、Arduino IDE 串行監視器、`screen`、`picocom`。

生產構建（`-e my-device-prod`，無 `IDRYER_DEV_REPL`）通過 Chrome 使用 Improv（`https://www.improv-wifi.com/`），並且不包含 REPL 代碼 — 該標誌是編譯時的，節省 Flash。

帶有 `WIFI_SSID/WIFI_PASSWORD` 的 `secrets.h`（第 2 步）對無頭 CI/自動刷新場景保持單獨的路徑 — 在兩個環境中都可用。

任何示例啟動並運行後，請閱讀：

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — `main.cpp` 中的對象順序。
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — 數據如何移動。
- [04-patterns/](../04-patterns/) — 食譜：添加傳感器、外設、傳輸。
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — 新產品的完整檢查清單。
- [10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — 如果棧被卡住怎麼辦。
