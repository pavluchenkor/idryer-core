# 自定义遥测（产品特定 payload）

## 何时使用

idryer-core 的标准遥测只发布通用合约中定义的字段（`units[].temperature`、`humidity`、`heaterPower` 等）。如果产品需要添加顶层 JSON 字段（例如 `outputMode`、`targetTempC`、`active`），或包含 `Telemetry` 结构中没有的数据，请使用这个方案。

典型场景：iHeater Link 会在标准 `units[]` 旁边发布 `outputMode` 和 `targetTempC`，这样后端就能通过 `telemetry:update` WebSocket 事件把 `heaterIntent` 转发给前端。

## 步骤 1 — 禁用自动发布

在 `Config` 中设置 `telemetryPeriodMs = 0`。这会阻止 idryer-core 自行发布精简版 payload：

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // publish manually
    .statusPeriodMs    = 5000,
};
```

## 步骤 2 — 编写发布函数

使用 `device().mqttClient()->publishTelemetry(doc)`。包含后端期望的所有字段：产品特定字段（顶层）以及标准 `units[]` 块。

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Current hardware output intent
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Active integration ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Product-specific top-level fields
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Standard units[] block — backend stores history from this
    // temperature/humidity = 0 if the device has no sensors
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // timestamp is added automatically
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

## 不要做的事

- **不要同时发布两份遥测**：不要在非零 `telemetryPeriodMs` 的 idryer-core 自动遥测之外再发布自定义遥测。后端会在同一个 topic 上收到两条消息并都处理，导致数据重复。
- **当 `telemetryPeriodMs = 0` 时不要调用 `device().publishTelemetryNow()`** — 它会发布不含产品特定字段的标准精简 payload。

## 为什么库不自己做这件事

idryer-core 已经在 `units[]` 中发布 `heaterPower: 1`，形式上足以判断加热是否处于活动状态。问题不在库，而在后端（`telemetry.handler.ts`）：它专门查找顶层 `outputMode` 字段，而不是从标准 `heaterPower` 推导 `heaterIntent`。这是后端侧的技术债。

当前方案是临时 workaround。如果后端改为从 `units[0].heaterPower` 推导 `heaterIntent`，就可以恢复 `telemetryPeriodMs = 5000` 并删除 `publishCustomTelemetry()`，标准库遥测会无需修改地工作。

关注 `telemetry.handler.ts` 的更新：一旦那里添加基于 `heaterPower` 的 fallback，此方案就不再需要。
