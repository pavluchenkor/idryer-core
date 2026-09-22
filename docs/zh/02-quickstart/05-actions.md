# 无模式的操作：继电器和呼叫设备

完成本页后，卡片上会有以分钟为单位的 **Ventilate** 表单，标题栏中有呼叫设备的按钮。

## 你需要

- 引脚 5 上一个可由 3.3 V 控制的继电器模块（或晶体管开关）；
- 接在继电器上的风扇或其他负载。

!!! warning
    230 V 市电负载必须有绝缘、保险丝和外壳。没有市电经验时，请用低压负载搭建。

## 代码

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // “风扇开/关”单元格
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 表示继电器关闭
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // 开/关的变化会立即发布
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // 每 250 ms
    if (s_ventUntilMs && (int32_t)(millis() - s_ventUntilMs) >= 0) {
        s_ventUntilMs = 0;
        digitalWrite(RELAY_PIN, LOW);
        s_link.telemetry.fanOn[0] = false;
    }
    if (s_blinks) digitalWrite(LED_PIN, --s_blinks % 2);
}

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // 无模式：单元不会变为“占用”，表单留在卡片上。
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // 卡片标题栏中的呼叫设备按钮。
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## 工作原理

- 无模式的操作（`mode` = `nullptr`）不会让单元变为“占用”：卡片不显示会话区块，表单留在原处。
- purpose `duration` 是以分钟为单位的时间：门户和应用会自动为该字段加上时间标签，范围来自 `.param(...)`。
- `hasFan` 提供“风扇开/关”单元格。`telemetry.fanOn` 的变化由核心立即发布，不等周期。
- `.deviceClass("identify")` 是卡片标题栏中常驻的呼叫设备按钮；`clear_errors` 用于清除错误。

## 检查

在卡片上设置时间并点击 **Ventilate**：继电器吸合，风扇单元格显示“开”。标题栏的呼叫按钮会让板载 LED 闪烁。

## 下一步

[PWM 加热](06-pwm.md)。
