# 步骤 06 — 用 PWM 替换 RMT

完成此步骤后，相同的门户命令流将驱动 PWM 输出而不是 RMT。典型的使用场景是通过 MOSFET 或 DC 调光器控制的加热器。

## 工作原理

执行器是一个普通的回调函数。前一步的 `RmtOutputAdapter` 是一个实现。用 `ledcWrite` 代码替换它——其他一切（MQTT、命令、状态）保持不变。

## 步骤

**1. 从 `main.cpp` 中移除** `RmtOutputAdapter` 的包含和实例：

```cpp
// 移除：
#include "controller/RmtOutputAdapter.h"
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. 在 `setup()` 中添加 PWM 初始化**：

```cpp
#define PWM_PIN     0      // MOSFET 栅极的 GPIO
#define PWM_CHANNEL 0      // LEDC 通道（0–15）
#define PWM_FREQ_HZ 25000  // 25 kHz — 大多数加热器无法听见
#define PWM_RES     8      // 8 位 → 占空比 0–255

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // 启动时关闭
```

**3. 在命令处理程序中**将 `s_output.apply(cmd)` 替换为 `ledcWrite`：

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

**4. `loop()` 不变**：

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` 是 3.x 之前版本的 Arduino ESP32 API。在 3.x 及更高版本中，使用 `ledcAttach(pin, freq, resolution)` 和 `ledcWrite(pin, duty)`。在 `platformio.ini` 中检查您的版本（`platform = espressif32@X.Y.Z`）。

## 验证

在门户上按下 **Heat** 按钮。输出引脚将携带一个 PWM 信号，其占空比与 `power` 参数成正比。用万用表（平均电压）或示波器验证。

## 接下来

- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — 完整的 `iDryer::Link` API 参考。
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — 任何新执行器的模板。
