# PWM 加热：模式、会话、功率

完成本页后，卡片可以用温度和时间启动加热，显示会话和功率，设备则根据传感器用 PWM 输出保持温度。

## 你需要

- [遥测一步](03-telemetry.md) 中的 SHT31 传感器；
- 引脚 3 上一个逻辑电平 MOSFET（3.3 V 即可导通），以及匹配其供电电压的加热器；
- 在 `platformio.ini` 中：`robtillaart/SHT31 @ ^0.5.0`。

!!! warning
    没有热熔断器的加热器不要无人看管：固件可能卡死，MOSFET 也可能击穿短路。

## 代码

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // MOSFET 栅极
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // 占空比 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // 加热功率单元格
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "PWM Heater",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);

static void readSensor() {
    const bool ok = s_sht.read();
    s_link.telemetry.airTempC[0]       = ok ? s_sht.getTemperature() : NAN;
    s_link.telemetry.airHumidityPct[0] = ok ? s_sht.getHumidity()    : NAN;
}

static uint32_t s_startMs = 0;

static void stopHeat(uint8_t unit) {
    ledcWrite(PWM_CHANNEL, 0);
    s_link.telemetry.heaterPower01[unit] = 0.0f;
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 表示不限时
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// 每秒一次：按与设定值的差计算功率，更新会话时间。
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // 没有读数就关闭加热器：绝不盲目加热。
    float power = 0.0f;
    if (!isnan(t)) power = constrain((s_link.status.targetTempC[0] - t) * 0.1f, 0.0f, 1.0f);
    ledcWrite(PWM_CHANNEL, (uint32_t)(power * 255));
    s_link.telemetry.heaterPower01[0] = power;

    const uint32_t elapsed = (millis() - s_startMs) / 1000;
    s_link.status.elapsedS[0] = elapsed;
    if (s_link.status.durationS[0] && elapsed >= s_link.status.durationS[0]) stopHeat(0);
}

void setup() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(8, 9);
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");

    s_link.every(2000, readSensor);
    s_link.every(1000, regulate);
}

void loop() {
    s_link.loop();
}
```

## 工作原理

- 模式为 `HEATING` 的操作会启动会话：回调设置模式、设定值和时长，并调用 `publishStatusNow()`。卡片按模式显示会话区块和 **Stop**。
- 门户和应用会自动为 `target_temperature` 和 `duration` 这两个 purpose 加标签。`args` 中的数值已被限制在参数范围内。
- `durationS = 0` 表示不限时；卡片把 `elapsedS` 显示为已用时间。
- `hasHeater` 提供功率单元格。工作时，核心发送最近两个发布周期内 `heaterPower01` 的平均值，所以 PWM 的频繁变化不会让卡片上的数值跳动。
- 这里的调节器是比例式：比设定值低 10 °C 即为满功率。需要精确保持时，换成 PID。
- `ledcSetup` / `ledcAttachPin` 是 arduino-esp32 2.x 的 API。在 3.x 中使用 `ledcAttach(pin, freq, bits)` 和 `ledcWrite(pin, duty)`。

## 检查

在卡片上启动 **Heat**：出现带设定值和时间的会话区块，功率单元格显示加热器的开启比例。**Stop** 关闭输出。

## 下一步

- [设备卡片：card 清单](../09-add-product/02-add-widget.md)：来自设备菜单的参数、配置文件、卡片布局。
- [如何添加新产品](../09-add-product/01-add-new-product.md)。
