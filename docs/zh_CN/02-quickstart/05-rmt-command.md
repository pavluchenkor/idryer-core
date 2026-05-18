# 步骤 05 — 门户命令：RMT 输出

完成此步骤后，在门户上按下开始按钮将在 ESP32 输出引脚上生成 RMT 脉冲。该示例遵循 iHeater Link，其中引脚通过光耦极驱动 iHeater STM32。

## 工作原理

门户将 `invoke` 命令发布到 MQTT 主题 `idryer/{serial}/commands/invoke`。库反序列化 JSON 并调用注册的处理程序。处理程序将命令传递给 `RmtOutputAdapter`，它在选定的引脚上生成脉冲帧。

处理程序独立于特定的引脚或协议——它是一个普通的回调函数。RMT 是一个实现；PWM 是另一个，请参阅 [06-pwm.md](06-pwm.md)。

## 需要什么

- ESP32-C3 或 ESP32（RMT 在所有 GPIO 引脚上可用）
- 输出引脚上的负载（在 iHeater Link 中——通过光耦极的 STM32）

## 步骤

**1. 在 `main.cpp` 中声明 RmtOutputAdapter**。基于 [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp)：

```cpp
#include "controller/RmtOutputAdapter.h"

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

默认输出引脚是 `IHEATER_TRIGGER_OUTPUT_PIN`。通过 `build_flags` 设置它：

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. 在 `setup()` 中初始化**：

```cpp
s_output.begin();
```

`begin()` 配置 RMT 通道并启动发送保活帧的后台 FreeRTOS 任务。

**3. 在 `setup()` 中注册命令处理程序**：

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

**4. 在 `loop()` 中——仅调用 `device().loop()`**：

```cpp
void loop() {
    device().loop();
}
```

RMT 帧从 `s_output` 内的 FreeRTOS 任务发送，独立于 `loop()`。

## 门户如何发送命令

门户发布到 MQTT 主题 `idryer/{serial}/commands/invoke`：

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

库接收此消息并使用反序列化的 `JsonObjectConst data` 调用已注册的回调。`action` 字段确定要执行的操作。

每个设备类型的操作列表在 [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 的 `invoke_actions` 下定义。

## 验证

打开门户 → 设备页面 → 按下 **Heat** 按钮。在串行监视器中：

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

RMT 脉冲将出现在输出引脚上（用示波器或逻辑分析仪验证）。

## 接下来

- [06-pwm.md](06-pwm.md) — 用 PWM 替换 RMT（MOSFET、DC 调光器）。
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — RMT 配置：脉冲频率、关闭代码、温度范围。
