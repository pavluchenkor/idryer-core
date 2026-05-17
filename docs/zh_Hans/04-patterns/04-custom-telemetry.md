# 自定义遥测（产品特定的有效负载）

## 何时使用

idryer-core 的标准遥测仅发布在通用合约中定义的字段（`units[].temperature`、`humidity`、`heaterPower` 等）。如果您的产品需要添加顶级 JSON 字段（例如 `outputMode`、`targetTempC`、`active`）或包含不存在于 `Telemetry` 结构中的数据，使用这个配方。

典型情况：iHeater Link 发布 `outputMode` 和 `targetTempC` 以及标准 `units[]`，这样后端可以通过 `telemetry:update` WebSocket 事件将 `heaterIntent` 转发到前端。

## 步骤 1 — 禁用自动发布

在 `Config` 中设置 `telemetryPeriodMs = 0`。这防止 idryer-core 发布自己的精简有效负载：

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // 手动发布
    .statusPeriodMs    = 5000,
};
```

## 步骤 2 — 编写发布函数

使用 `device().mqttClient()->publishTelemetry(doc)`。包含后端期望的所有字段：产品特定的（顶级）和标准的 `units[]` 块。

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // 当前硬件输出意图
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // 活跃的集成（'bambu' / 'moonraker' / 'ha' / 'none'）
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // 产品特定的顶级字段
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // 标准的 units[] 块 — 后端从这里存储历史
    // temperature/humidity = 0 如果设备没有传感器
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // 时间戳自动添加
}
```

## 步骤 3 — 从 `loop()` 调用

```cpp
void loop() {
    device().loop();

    static uint32_t s_lastTelMs = 0;
    if ((uint32_t)(millis() - s_lastTelMs) >= 5000u) {
        s_lastTelMs = millis();
        publishCustomTelemetry();
    }
    // ...
}
```

## 不要做什么

- **不要同时发布** idryer-core 自动遥测（非零 `telemetryPeriodMs`）和自定义遥测。后端在同一主题上接收两条消息并处理两条——数据被重复。
- **当 `telemetryPeriodMs = 0` 时不要调用 `device().publishTelemetryNow()`** — 它发布没有您产品特定字段的标准精简有效负载。

## 为什么库不自己做这个

idryer-core 已经在 `units[]` 内发布 `heaterPower: 1` — 正式足够知道加热是活跃的。问题不在库中而在后端（`telemetry.handler.ts`）：它特别查找顶级 `outputMode` 字段而不是从标准 `heaterPower` 推导 `heaterIntent`。这是后端的技术债务。

当前的配方是一个临时变通办法。如果后端被修复为从 `units[0].heaterPower` 推导 `heaterIntent`，您可以恢复到 `telemetryPeriodMs = 5000` 并移除 `publishCustomTelemetry()` — 标准库遥测无需任何更改即可工作。

留意 `telemetry.handler.ts` 的更新：一旦在那里添加了对 `heaterPower` 的回退，这个配方就变得多余了。
