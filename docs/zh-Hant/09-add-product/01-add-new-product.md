---
title: "如何新增基於 idryer-core 的新產品"
description: "基於 iDryer::Link 外觀類別的新 iDryer 裝置清單：專案、最小韌體、在應用程式中設定 Wi-Fi 和綁定、遙測、設定選單、裝置卡片、合約。"
---

# 如何新增基於 idryer-core 的新產品

當你在 `idryer-core` 上開發新產品（線材烘乾機、加熱模組、照明、感測器或其他模組）時，請使用本指南。它說明核心庫替你完成了什麼，以及產品程式碼需要補充什麼。

一個可以完整編譯的範例是 Build-Your-Own-iDryer 文件中的加熱儲料櫃（`example/09-cabinet`）：它涵蓋了本頁的全部內容。

---

## 產品的基礎

產品透過一個物件與核心庫互動——外觀類別 `iDryer::Link`（`<iDryer.h>`）。在 `s_link.begin()` 和 `s_link.loop()` 中，核心庫會：

- 以無線方式（ESPTouch）從 iDryer 應用程式、或透過 USB（Improv）從網頁安裝程式取得 Wi-Fi 網路，並保持連線；
- 把裝置綁定到帳戶：等待一次性綁定權杖——來自應用程式（經區域網路）或來自序列埠（`PAIR_TOKEN:<token>`）——並在門戶上換取永久金鑰；
- 連接 MQTT，並依 `Config` 中的週期發布 `telemetry` 和 `status`；
- 在區域網路中廣播自己（mDNS `_idryer._tcp`），並透過 WebSocket 接收應用程式的指令；
- 發布裝置卡片的 card 清單。

更底層的類別（`IdryerRuntime`、`CloudStateMachine`、`LocalAccess` 等）是核心庫的內部實作：綁定權杖只有在 `iDryer::Link` 內部才會傳到雲端部分。請在外觀類別之上建立產品。

---

## 1. 專案

`idryer-core` 放在 `lib/idryer-core/`（複製或符號連結）；PlatformIO 從它的 `library.json` 取得核心庫的相依套件。最小的 `platformio.ini`：

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; espMqttClient 相依套件中的 ESP8266 傳輸層：無法在 ESP32 上編譯
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

沒有 `lib_ignore = ESPAsyncTCP`，建置會在 `ESPAsyncTCP.cpp` 處失敗；沒有 `MQTT_BROKER` 和 `MQTT_PORT`，核心庫無法編譯。

---

## 2. 最小韌體

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 不是 iDryer 產品：卡片來自清單
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // 你的感測器函式
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` 啟動 Wi-Fi、綁定、MQTT 和本機存取；`s_link.loop()` 必須一直執行，不能使用 `delay()`。當裝置從帳戶解除綁定時，門戶會送來 `revoke` 指令：`handleRevoke()` 清除金鑰，裝置等待新的綁定權杖。

---

## 3. Wi-Fi 和綁定——不寫在程式碼裡

韌體中既沒有網路密碼，也沒有帳戶資料。使用者在 iDryer 應用程式中連接裝置：**连接新设备** → **Wi-Fi** 步驟（應用程式透過 ESPTouch 傳送網路）→ **绑定** 步驟（應用程式透過 mDNS 找到裝置，從門戶取得一次性權杖並交給裝置；裝置自己在門戶上啟用權杖）。在 Wi-Fi 連上之前，序列埠不會輸出：核心庫把它留給網頁安裝程式（Improv）。

分步說明和預期日誌——Build-Your-Own-iDryer，「在核心庫上啟動韌體」一章。

---

## 4. 資料：遙測和狀態

- `Config` 中的 `has*` 旗標決定哪些詞彙欄位進入遙測、卡片上出現哪些儲存格。
- 把值寫入 `s_link.telemetry`（`airTempC`、`airHumidityPct`、`heaterTempC`、`heaterPower01`、`fanOn`、`servoOpen`）和 `s_link.status`（`mode`、`targetTempC`、`durationS`、`elapsedS`）；核心庫依 `Config` 中的週期發布它們，`s_link.publishStatusNow()` 會立即傳送狀態。
- 自訂欄位——透過 `s_link.onTelemetryPublish()`，顯示到卡片上——透過 `s_link.card().sensor()`；見[裝置卡片](02-add-widget.md)。

---

## 5. 設定：選單

設定在 `src/menu/menu.yaml` 中描述；產生器 `menu_gen.py` 由此產生 C++ 程式碼、NVS 儲存和選單 JSON（[作為協定的選單](../08-contracts/02-menu-as-protocol.md)）。產品程式碼：

- 在 `s_link.begin()` 之前載入選單：`menu.initDefaults()`、`menu.loadFromNVS()`、`menu_sync_state_to_cache()`；
- 用 `menu_buildFullJson()` 和 `s_link.devicePublisher()->publishConfigRaw()` 發布選單——在上線時以及收到 `get_config` 指令時；
- 用 `menu_apply_by_bind()` 套用 `set` 指令（值、NVS 和快取一次完成），然後重新發布選單。

完整程式碼——Build-Your-Own-iDryer，「來自 YAML 的選單」一章。

---

## 6. 裝置卡片

卡片顯示什麼、啟動哪些操作，用 `s_link.card()` 宣告——[裝置卡片：card 清單](02-add-widget.md)。

---

## 7. 合約

當你新增新主題或修改負載時：

1. 更新 `contracts/mqtt_contract.yaml`；
2. 執行 `contracts/regen.sh` 並提交產生的檔案。

---

## 雙晶片裝置

對於透過 UART 與獨立控制器（例如 RP2040）協作的 ESP32，核心庫提供 UART 橋接 `idryer_uart.h`；可執行的參考實作是 `idryer-link` 韌體。
