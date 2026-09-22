# idryer-core 的運作方式

idryer-core 是一個 ESP32 函式庫。它負責把裝置連接到門戶和應用程式的全部工作：

- Wi-Fi：網路由 iDryer 應用程式（ESPTouch）或透過 USB 的網頁（Improv）傳給裝置；
- 用一次性權杖綁定到帳戶，以及解除綁定；
- 具備重新連線的安全 MQTT 工作階段；
- 區域網路存取：即使沒有網際網路，應用程式也能控制裝置；
- 發布遙測和狀態，傳遞命令；
- 無線韌體更新；
- 門戶和應用程式中的裝置卡片。

你只寫自己的部分：讀取感測器、驅動負載、宣告卡片顯示什麼以及它啟動哪些操作。

## 唯一的入口：`iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 你自己的裝置
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // 溫度儲存格
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // 你的程式碼
}
```

| 你 | 核心 |
|---|---|
| 填寫 `s_link.telemetry.*` | 每 30 秒發布一次，閒置時每 60 秒一次 |
| 修改 `s_link.status.*`（模式、設定值、時間）並呼叫 `publishStatusNow()` | 送達狀態；卡片依模式切換 |
| 宣告 `s_link.card()`：感測器、控制項、操作 | 產生 card 清單並發布 |
| 註冊 `s_link.onCommand(...)` | 轉交來自雲端和區域網路的命令 |

## 裝置卡片

門戶和應用程式依據裝置傳送的 card 清單繪製卡片：

- `Config.has*` 旗標提供現成的儲存格：溫度、濕度、加熱功率、風扇等；
- `card().sensor(...)` 依遙測中的路徑加入自訂數值；
- `card().action(...)` 宣告一個操作：執行後單元的模式和啟動參數。

門戶上不需要為此寫程式碼。詳見 [裝置卡片：card 清單](../09-add-product/02-add-widget.md)。

## `mqtt_contract.yaml` 是唯一的事實來源

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 描述協定：主題、遙測和狀態欄位、裝置能力、card 清單。由它產生：

| 內容 | 位置 |
|---|---|
| `iDryer::Config`（`has*` 旗標）和 API 結構 | `src/_generated/iDryer_api.h` |
| MQTT 主題 | `contracts/_generated/mqtt_topics.h` |
| ESP32 ↔ 控制器的 UART 協定 | `contracts/_generated/uart_protocol.h` |
| 門戶用的 TypeScript 型別 | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    不要手動修改 `_generated/` 中的檔案：`contracts/regen.sh` 會依據契約覆寫它們。

自訂數值不需要修改契約：用 `card().sensor(...)` 宣告即可。當所有產品都需要一項新能力時才修改契約：先改 `mqtt_contract.yaml`，再執行 `regen.sh`，最後改程式碼。

## 基於核心的產品

- **iDryer Link**：乾燥機的通訊模組，一塊 ESP32 緊鄰控制器，透過 UART 通訊。
- **iDryer Storage**：線盤架照明，可定址燈條和 SHT31 感測器。
- **iHeater Link**：控制 iHeater 加熱器，整合 Bambu Lab、Klipper/Moonraker 和 Home Assistant。

## 下一步

[5 分鐘執行](01-five-minutes.md)。
