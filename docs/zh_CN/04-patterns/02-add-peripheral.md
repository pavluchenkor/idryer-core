# 添加外围设备

## 何时使用

如果设备需要响应来自云或 LAN 的命令控制硬件——继电器、加热器、LED 条、马达——使用这个配方。

## 现成代码

```cpp
// main.cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::StorageLink,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "1.0.0",
};

static iDryer::Link s_link(CFG);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (!cmd) return;

    if (strcmp(cmd, "invoke") == 0) {
        const char* action = data["action"] | "";

        if (strcmp(action, "fan.on") == 0) {
            myFan.on();
            s_link.publishStatusNow();  // 立即反映新状态
            return;
        }
        if (strcmp(action, "fan.off") == 0) {
            myFan.off();
            s_link.publishStatusNow();
            return;
        }
    }

    if (strcmp(cmd, "drying") == 0) {
        float targetTempC  = data["targetTempC"]  | 45.0f;
        uint32_t durationS = data["durationS"]    | 0;
        myHeater.start(targetTempC, durationS);
        s_link.status.mode[0]        = iDryer::UnitMode::Drying;
        s_link.status.targetTempC[0] = targetTempC;
        s_link.status.durationS[0]   = durationS;
        s_link.publishStatusNow();
        return;
    }

    if (strcmp(cmd, "stop") == 0) {
        myHeater.stop();
        s_link.status.mode[0] = iDryer::UnitMode::Idle;
        s_link.publishStatusNow();
        return;
    }
}

void setup() {
    myFan.begin();
    myHeater.begin();
    s_link.begin();
    // 重要：setCommandHandler — 严格在 begin() 之后。
    // begin() 安装其自己的调度程序；我们的 handleCommand 必须覆盖它。
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## 解释

`s_link.runtime()->setCommandHandler(handleCommand)` 是命令处理程序的单一连接点。在此调用后，所有传入的 MQTT 命令（`invoke`、`set`、`drying`、`stop`、`ping`、`get_config` 等）直接到达 `handleCommand`。

`s_link.publishStatusNow()` — 在每次更改 `s_link.status.*` 后调用。这立即将新状态发送到门户和 LAN 客户端，无需等待 `statusPeriodMs` 计时器。

永远不要在 `handleCommand` 中调用 `delay()` — 该调用来自 MQTT 回调的同步；阻塞它会破坏会话。将计时器放在产品对象的 `loop()` 中。

### 替代方案：`link.onRequest()`

对于标准命令（`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`），通过 `onRequest()` 的更简单的回调就足够了——无需解析原始 JSON：

```cpp
s_link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start:
            myHeater.start(r.targetTempC, r.durationS);
            break;
        case iDryer::RequestKind::Stop:
            myHeater.stop();
            break;
        default:
            break;
    }
});
```

`onRequest()` 不能与 `setCommandHandler` 并存——如果设置了完整处理程序，`onRequest` 回调不会被调用。有关详细信息，请参阅 [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)。

## 仓库中的完整示例

参考实现：在 `iHeater-link/src/main.cpp` 中处理 `drying` / `stop` 的 `handleCommand`。
