# 通过 devicePublisher 发布

## 何时使用

`iDryer::Link` 已包含两个内置传输：MQTT（云）和本地 WebSocket（LAN）。大多数任务不需要额外的传输。

当产品组装其自己的有效负载并必须同时将其发送到两个通道时，使用 `s_link.devicePublisher()` ——例如，当响应 `commands/get_config` 发布菜单配置时。

## 现成代码

```cpp
// main.cpp（片段）
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// 在单个调用中将任意 JSON 有效负载发布到 MQTT 和本地 WS。
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // 产品函数
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

单个 `publishConfigRaw` 调用将有效负载传递到 MQTT 主题 `idryer/{serial}/config` 和所有活动的 LAN WS 客户端。无需创建其他客户端或主题。

## 解释

`devicePublisher()` 是外观的双重发布助手。使用它而不是直接调用 `mqttClient()` 或 `LocalAccess`，除非需要发布到非标准主题。

遥测和状态由外观在计时器上自动发布——`devicePublisher()` 不需要用于这些。

## 何时需要第三个传输

添加第三个通道（BLE、串行 JSON、UART 代理）是外观的架构扩展，而不是配方模式。绝大多数设备不需要这个。

如果确实需要——入口点在 `lib/idryer-core/src/cloud/`（云状态机、MQTT）和 `lib/idryer-core/src/`（本地访问）。在继续之前，确认内置的 MQTT 和本地 WS 对您的使用情况不足。

## 仓库中的完整示例

`iDryer-Storage/src/main.cpp:171` 中的 `publishFullMenu()` ——通过 `s_link.devicePublisher()->publishConfigRaw(buf, len)` 发布完整的 JSON 菜单。
