---
title: "裝置卡片：card 清單"
description: "基於 idryer-core 的韌體如何用 s_link.card() 描述自己的卡片：感測器、帶啟動參數的動作、送到門戶的內容，以及門戶和應用程式繪製的內容。"
---

# 裝置卡片：card 清單

門戶和行動應用程式根據裝置的 **card 清單**（card manifest）建立任何裝置的卡片。清單是韌體對自身的描述：顯示什麼、可以控制什麼。新的裝置類型不需要在門戶或應用程式中撰寫程式碼。

清單屬於 `iDryer::Link` 外觀類別（`s_link.card()`）。底層執行環境（`IdryerRuntime`）不會發布清單。

選單和卡片是兩回事。選單是裝置設定的鏡像：在門戶上修改的值會寫入裝置記憶體。卡片顯示量測值並啟動操作：啟動參數隨一條指令送出，不會寫入選單。啟動參數可以從選單項目取得範圍和預設值。

---

## 運作方式

```text
韌體：s_link.card() 宣告
   │  核心庫產生 JSON，以 retained（QoS 1）發布到 idryer/{key}/card
   ▼
門戶後端：驗證清單（上限、允許的類型和欄位）並儲存
   ▼
門戶和應用程式：繪製卡片
   │  使用者按下按鈕
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
核心庫把 "card.<id>" 交給你的回呼
```

MQTT 連線建立後，核心庫發布清單；當宣告或其依賴的選單項目改變時，重新發布。

---

## 實體：顯示什麼

生態系詞彙表中的感測器依 `Config` 旗標自動加入，無需宣告：

| `Config` 旗標 | 卡片儲存格 |
|---|---|
| `hasAirTemp` | 空氣溫度 |
| `hasAirHumidity` | 濕度 |
| `hasHeaterTemp` | 加熱器溫度 |
| `hasHeater` | 加熱功率 |
| `hasFan` | 風扇 開 / 關 |
| `hasServo` | 風門 開 / 關 |
| `hasWeight` | 秤重模組（topic `weights`） |
| `hasRfid` | 該單元有 RFID 讀取器 |

自訂數值：寫入遙測，並宣告帶 JSON 路徑的感測器：

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

簡單控制項也是實體：`button`、`number`、`select`。使用者一改值就會送出，核心庫呼叫你的回呼：

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## 動作：帶啟動參數的操作

動作是裝置的操作：開始烘乾、加熱、開燈、停止。動作有一個 **模式**——動作之後單元的模式（`status.units[].mode`）——以及帶有含義（`purpose`）的 **參數**。卡片依單元目前的模式決定顯示什麼：

- 單元的模式與某個動作的模式一致 → 裝置正在執行它：顯示工作階段區塊和停止按鈕（模式為 `IDLE` 的動作）；
- 否則 → 顯示啟動表單；有多個啟動動作時 → 顯示模式切換。

範例：用一塊 ESP32 製作的加熱儲料櫃。目標溫度是選單項目 `target_temp`（30–50 °C，預設 45）。

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // 已在 30..50 範圍內
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // 把選單值寫入卡片讀取的快取
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

回呼收到的內容：

- 數值已被限制在單元的範圍內，缺少的數值以預設值取代；
- `unit` —— 由 `unitId` 得到的單元索引（`"U1"` → 0）；送給裝置沒有的單元的指令會被忽略；
- 啟動後設定 `status.mode` 並呼叫 `publishStatusNow()`——卡片依狀態切換到工作階段區塊。

### 動作 API

| 呼叫 | 作用 |
|---|---|
| `card.action(id, mode, cb)` | 一個動作；`mode` —— 執行後單元的模式，`nullptr` —— 模式不變 |
| `.param(id, purpose, MENU_ID)` | 數值：範圍、步長、預設值和單位取自選單項目 |
| `.param(id, purpose, min, max, step, def[, unit])` | 自帶範圍的數值 |
| `.ceiling(MENU_ID)` | 前一個參數的上限取該選單項目的值（例如最高空氣溫度） |
| `.stages(id, "stages", MENU_ID)` | 曲線階段 `[{temperature, ramp, hold}]`，單位秒；階段溫度在選單項目範圍內 |
| `.select(id, purpose, options, count[, def])` | 從清單中選擇；清單外的值以預設值取代 |
| `.color(id, purpose, "#FFFFFF")` | 顏色 `#RRGGBB` |
| `.deviceClass("identify")`、`.deviceClass("clear_errors")` | 卡片標頭的常駐按鈕：定位裝置、清除錯誤 |
| `.name("ru", "…").name("en", "…")` | 卡片不認識該模式時顯示的動作名稱 |

`purpose` 取值：`target_temperature`、`target_humidity`、`duration`（分鐘，0 —— 不限時）、`stages`、`start_stage`（從 0 開始）、`effect`、`rgb_color`。卡片用它們為欄位加標籤並接上門戶功能：烘乾預設填入模式為 `DRYING` 的動作的 `target_temperature` 和 `duration`，烘乾曲線填入 `stages`。

回呼是函式或無擷取的 lambda。`name` 字串和選項以指標形式保存——請使用字面值或靜態陣列。

---

## 送到門戶的內容

上面的儲料櫃發布以下內容（`Config`：`hasAirTemp`、`hasAirHumidity`、`hasHeaterTemp`、`hasHeater`、`hasFan`）：

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` 和 `default` 來自選單項目。使用 `.ceiling()` 時，參數還會帶有 `max_by`——壓低上限的那個選單項目的標題；數值超過上限時，卡片會指出它。

---

## 門戶和應用程式繪製的內容

示意圖，不是截圖。閒置時：

```text
┌─ DIY Storage Cabinet ──────────────── [Idle] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                 │  ← 溫度、濕度、加熱器
│ | 0 %     | off             |                 │  ← 功率、風扇（灰色：0 / 關）
│ [Temp. 45 °C          ]  [Storage]            │  ← 動作 "storage"
└───────────────────────────────────────────────┘
```

啟動後，裝置回報模式 `STORAGE`，同一張卡片顯示工作階段區塊（目標溫度和已存放時間）和停止按鈕——模式為 `IDLE` 的動作。

兩端共同的規則：

- 沒有數值的儲存格顯示為灰色；功率為 0 時也是灰色；風扇和風門在關閉狀態時為灰色；
- 超出範圍的值不會被取代：啟動被阻擋，表單下方顯示原因；
- 對於清單中有動作的韌體，只有宣告了 `identify` / `clear_errors` 動作，才會出現定位和清除錯誤按鈕。

卡片出現的位置：

| | 門戶 | 應用程式 |
|---|---|---|
| 非 iDryer 產品的裝置類型 | 儀表板上依清單產生的卡片，帶動作；裝置頁面 —— 讀數和動作（清單有動作時） | 首頁 —— 讀數；動作 —— 在裝置頁面 |
| `deviceType = Dryer` | 帶同樣清單動作的烘乾機卡片 | 首頁 —— 監控；動作 —— 在裝置頁面 |

---

## 上限

| | 核心庫 | 門戶後端 |
|---|---|---|
| 實體 | 宣告 16 個 + 自動 | 32 |
| layout 列數 | 8 | 16 |
| 每列 id 數 | 4 | 4 |
| 動作 | 8 | 8 |
| 每個動作的參數 | 4（`IDRYER_CARD_MAX_PARAMS`） | 6 |
| `name` 語言 | `ru`、`en` | 最多 4 |

`id`：小寫拉丁字母、數字和 `_`，最多 24 個字元；實體和動作共用一個命名空間。清單文件上限為 4096 位元組；放不下時，核心庫在日誌中輸出 `manifest overflows`，並且不發布。

格式本身 —— `contracts/mqtt_contract.yaml`，`mqtt_only` 部分，`suffix: card`。
