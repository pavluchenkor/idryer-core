# 第 06 步 — 用 PWM 取代 RMT

完成本步驟後，相同的入口網站命令流將驅動 PWM 輸出而不是 RMT。典型用例是通過 MOSFET 或 DC 調光器控制的加熱器。

## 工作原理

執行器是一個普通的回調函數。上一步的 `RmtOutputAdapter` 是一種實現。用 `ledcWrite` 代碼取代它 — 其他所有內容（MQTT、命令、狀態）保持不變。

## 步驟

**1. 從 `main.cpp` 中移除** `RmtOutputAdapter` 包含和實例：

```cpp
// 移除：
#include "controller/RmtOutputAdapter.h"
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. 在 `setup()` 中添加 PWM 初始化**：

```cpp
#define PWM_PIN     0      // MOSFET 柵極的 GPIO
#define PWM_CHANNEL 0      // LEDC 通道 (0–15)
#define PWM_FREQ_HZ 25000  // 25 kHz — 對大多數加熱器不可聽
#define PWM_RES     8      // 8 位 → 佔空比 0–255

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // 啟動時關閉
```

**3. 在命令處理程序中**用 `ledcWrite` 取代 `s_output.apply(cmd)`：

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action   = data["action"] | "";
    JsonObjectConst args = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float power01 = args["power"] | 1.0f;  // 0.0–1.0
        uint8_t duty  = (uint8_t)(power01 * 255.0f);
        ledcWrite(PWM_CHANNEL, duty);

        device().status.mode[0]             = iDryer::UnitMode::Drying;
        device().telemetry.heaterPower01[0] = power01;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        ledcWrite(PWM_CHANNEL, 0);

        device().status.mode[0]             = iDryer::UnitMode::Idle;
        device().telemetry.heaterPower01[0] = 0.0f;
        device().publishStatusNow();
    }
});
```

**4. `loop()` 不會改變：**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` 是 3.x 之前版本的 Arduino ESP32 API。在 3.x 及以上版本中使用 `ledcAttach(pin, freq, resolution)` 和 `ledcWrite(pin, duty)`。檢查 `platformio.ini` 中的版本（`platform = espressif32@X.Y.Z`）。

## 驗證

在入口網站上按下 **Heat** 按鈕。輸出引腳將攜帶 PWM 信號，佔空比與 `power` 參數成正比。用萬用表（平均電壓）或示波器驗證。

## 後續步驟

- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — 完整的 `iDryer::Link` API 參考。
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — 任何新執行器的模板。
