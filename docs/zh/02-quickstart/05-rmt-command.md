# 第 05 步 — 入口網站命令：RMT 輸出

完成本步驟後，在入口網站上按下「開始」按鈕將在 ESP32 輸出引腳上產生 RMT 脈衝。本示例遵循 iHeater Link，其中引腳通過光耦合器驅動 iHeater STM32。

## 工作原理

入口網站將 `invoke` 命令發佈到 MQTT 主題 `idryer/{serial}/commands/invoke`。庫反序列化 JSON 並調用已註冊的處理程序。處理程序將命令傳遞給 `RmtOutputAdapter`，後者在選定的引腳上生成脈衝幀。

處理程序獨立於特定的引腳或協議 — 它是一個普通的回調函數。RMT 是一種實現；PWM 是另一種實現，請參閱 [06-pwm.md](06-pwm.md)。

## 所需條件

- ESP32-C3 或 ESP32（RMT 在所有 GPIO 引腳上可用）
- 輸出引腳上的負載（在 iHeater Link 中 — 通過光耦合器連接的 STM32）

## 步驟

**1. 在 `main.cpp` 中聲明 RmtOutputAdapter**。基於 [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp)：

```cpp
#include "controller/RmtOutputAdapter.h"

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

默認輸出引腳是 `IHEATER_TRIGGER_OUTPUT_PIN`。通過 `build_flags` 設定它：

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. 在 `setup()` 中初始化**：

```cpp
s_output.begin();
```

`begin()` 配置 RMT 通道並啟動一個發送保活幀的後臺 FreeRTOS 任務。

**3. 在 `setup()` 中註冊命令處理程序**：

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action    = data["action"] | "";
    JsonObjectConst args  = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float    tempC  = args["tempC"]      | 0.0f;
        uint32_t durMin = args["durationMin"] | 0u;

        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = tempC;
        s_output.apply(cmd);

        device().status.mode[0]        = iDryer::UnitMode::Drying;
        device().status.targetTempC[0] = tempC;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::Off;
        cmd.targetTempC = 0.0f;
        s_output.apply(cmd);

        device().status.mode[0] = iDryer::UnitMode::Idle;
        device().publishStatusNow();
    }
});
```

**4. 在 `loop()` 中 — 僅調用 `device().loop()`：**

```cpp
void loop() {
    device().loop();
}
```

RMT 幀從 `s_output` 內的 FreeRTOS 任務發送，獨立於 `loop()`。

## 入口網站如何發送命令

入口網站發佈到 MQTT 主題 `idryer/{serial}/commands/invoke`：

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

庫接收此消息並使用反序列化的 `JsonObjectConst data` 調用已註冊的回調。`action` 字段確定要執行的操作。

每種設備類型的操作列表在 [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 中的 `invoke_actions` 下定義。

## 驗證

打開入口網站 → 設備頁面 → 按下 **Heat** 按鈕。在串行監視器中：

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

RMT 脈衝將出現在輸出引腳上（用示波器或邏輯分析儀驗證）。

## 後續步驟

- [06-pwm.md](06-pwm.md) — 用 PWM 取代 RMT（MOSFET、DC 調光器）。
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — RMT 配置：脈衝頻率、Off 代碼、溫度範圍。
