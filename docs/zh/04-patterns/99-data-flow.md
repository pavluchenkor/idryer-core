# 参与者之间的数据流

这是应用层面的章节：说明传感器、外设、profile、transport 和 publisher 如何在真实产品代码中连接。架构级数据流说明见 [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md)。

## 原理

`idryer-core` 有意不提供内部事件总线。参与者之间的所有连接都是在组合根中通过构造函数传递的**显式指针**。这意味着：

- 任何数据流都可以在 `main.cpp` 中读成一条指针链。
- 没有“魔法式”的参与者发现。
- 产品决定谁把什么传给谁。

## Storage Link 的典型连接图

```
   Sensor (Sht31ClimateSensor)
        │
        │ tick(now), get()
        ▼
   StorageTelemetryPublisher    ──→  DevicePublisher  ──→  MqttClient + LocalAccess
                                                            │
                                                            ▼
                                                       broker / WS-client


   handleCommand   ←──  IdryerRuntime   ←──  MqttClient (commands/*)
        │           ←──  LocalAccess    ←──  WS-client (envelope)
        │
        ├──→  ActionDispatcher  ──→  LedStripExecutor (peripheral)
        ├──→  IProfile::getConfig  ──→  DevicePublisher::publishConfig
        └──→  IProfile::applyConfig (via onSetCommand)
```

每个箭头都是 `main.cpp` 中一行指针传递。例如：

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     publisher
```

## 方案 1 — 传感器发布到云端

**目标**：温度传感器 → MQTT。

```
Sensor → Publisher → DevicePublisher → MqttClient + LocalAccess
```

```cpp
static MySensor              s_sensor;
static MyTelemetryPublisher  s_telemetry(&s_sensor, &s_pub);

void loop() {
    s_runtime.loop();
    s_local.loop();
    s_sensor.tick(millis());
    s_telemetry.loop(millis());
}
```

`MyTelemetryPublisher::loop` 决定何时发布（按间隔）。见 [01-add-sensor.md](01-add-sensor.md)。

## 方案 2 — 云端命令 → 外设

**目标**：`commands/invoke {"action":"led.pulse",...}` → 打开 LED。

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onInvoke → LedStripExecutor
```

```cpp
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    return s_executor.execute(action, args);
}

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    // ...
}

void setup() {
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_runtime.setCommandHandler(handleCommand);
    // ...
}
```

见 [02-add-peripheral.md](02-add-peripheral.md)。

## 方案 3 — LAN 应用命令 → 外设（同一路径）

**目标**：LAN 上的 WS 客户端发送 `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → 同一个 LED 打开。

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

不需要新代码：`s_local.setCommandSink(handleCommand)` 已经把两种传输合并到同一个 handler。

## 方案 4 — 传感器 → 外设（内部 loop）

**目标**：传感器读取湿度 → 如果高于阈值则打开风扇。

这是产品内部逻辑；`idryer-core` 没有为这种连接提供 API。直接实现即可：

```cpp
class HumidityController {
public:
    HumidityController(IClimateSensor* sensor, Fan* fan, float threshold)
        : sensor_(sensor), fan_(fan), threshold_(threshold) {}

    void loop(uint32_t nowMs) {
        if (nowMs - lastCheckMs_ < 5000) return;
        lastCheckMs_ = nowMs;

        SensorReading r = sensor_->get();
        if (!r.ok) return;
        if (r.humidity > threshold_)  fan_->on();
        else                          fan_->off();
    }
private:
    IClimateSensor* sensor_;
    Fan*    fan_;
    float           threshold_;
    uint32_t        lastCheckMs_ = 0;
};
```

在组合根中连接：

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` 不知道这个类，也不应该知道。

## 方案 5 — 配置变化 → 外设重新初始化

**目标**：后端发送 `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → LED 亮度立即变化。

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Peripheral
```

```cpp
class MyProfile : public idryer::IProfile {
public:
    MyProfile(MyDevice* a) : device_(a) {}

    bool applyConfig(int id, int val) override {
        if (id == CFG_BRIGHTNESS) {
            menu.brightness = val;
            menu.saveToNVS();
            device_->setBrightness(val);   // immediate apply
            return true;
        }
        return false;
    }
    // ...
private:
    MyDevice* device_;
};
```

`profile → peripheral` 连接在组合根中建立：

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## 方案 6 — 新事件 → events topic

**目标**：外设捕获错误 → `idryer/{serial}/events` 中的事件。

外设不会自己发布。它通知产品，由产品发布：

```cpp
class MyDevice {
public:
    using ErrorCallback = std::function<void(int errCode, const char* msg)>;
    void setErrorCallback(ErrorCallback cb) { errCb_ = cb; }
    // ...
private:
    ErrorCallback errCb_;
    void reportError(int code, const char* msg) {
        if (errCb_) errCb_(code, msg);
    }
};

// in main.cpp
s_device.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

或者，外设也可以通过构造函数接收 `DevicePublisher*`。关键点是：连接必须显式。

## 我们不做的事

- 不引入内部事件总线。它会带来隐藏连接和调试复杂度。
- 不把传感器、外设、publisher 收集到共享的 `IDeviceContainer` 中。连接只在组合根中精确建立。
- 不使用基于名称的订阅（例如“publisher 'telemetry' 监听 sensor 'sht31'”）。所有连接都是有类型的指针。

## 相关文档

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — 创建和组装顺序。
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — 架构图。
- [04-patterns/01-add-sensor.md](01-add-sensor.md)、[02-add-peripheral.md](02-add-peripheral.md)、[03-add-transport.md](03-add-transport.md) — 具体组件方案。
