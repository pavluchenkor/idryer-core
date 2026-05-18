# 参与者之间的数据流

应用部分：传感器、外围设备、配置文件、传输和发布者如何在真实产品代码中连接。架构数据流说明在 [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) 中。

## 原则

`idryer-core` 有意不提供内部事件总线。所有参与者之间的连接都是通过组合根中的构造函数传递的**显式指针**。这意味着：

- 任何数据流都可以读作 `main.cpp` 中的指针链。
- 没有"魔法"参与者发现。
- 产品决定谁传递什么给谁。

## 存储链接的典型连接图

```
   传感器（Sht31ClimateSensor）
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

每个箭头都是 `main.cpp` 中的一条指针传递线。例如：

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     publisher
```

## 配方 1 — 传感器发布到云

**目标**：温度传感器 → MQTT。

```
传感器 → 发布者 → DevicePublisher → MqttClient + LocalAccess
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

`MyTelemetryPublisher::loop` 决定何时发布（按间隔）。请参阅 [01-add-sensor.md](01-add-sensor.md)。

## 配方 2 — 云命令 → 外围设备

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

请参阅 [02-add-peripheral.md](02-add-peripheral.md)。

## 配方 3 — LAN 应用程序命令 → 外围设备（相同路径）

**目标**：LAN 上的 WS 客户端发送 `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → 同一个 LED 打开。

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

不需要新代码 — `s_local.setCommandSink(handleCommand)` 已经将两个传输合并到一个处理程序中。

## 配方 4 — 传感器 → 外围设备（内部循环）

**目标**：传感器读取湿度 → 如果超过阈值，风扇打开。

这是内部产品逻辑；`idryer-core` 没有这样连接的 API。直接做：

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

## 配方 5 — 配置更改 → 外围设备重新初始化

**目标**：后端发送 `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → LED 亮度立即改变。

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
            device_->setBrightness(val);   // 立即应用
            return true;
        }
        return false;
    }
    // ...
private:
    MyDevice* device_;
};
```

`profile → peripheral` 连接在组合根中构建：

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## 配方 6 — 新事件 → 事件主题

**目标**：外围设备捕获错误 → `idryer/{serial}/events` 中的事件。

外围设备不自己发布。它通知产品；产品发布：

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

// 在 main.cpp 中
s_device.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

或者，外围设备可以通过其构造函数接受 `DevicePublisher*`。关键点：连接是明确的。

## 我们不做什么

- 我们不引入内部事件总线。这会导致隐藏的连接和调试复杂性。
- 我们不将传感器/外围设备/发布者收集到共享的 `IDeviceContainer` 中。连接在组合根中精确构建。
- 我们不使用基于名称的订阅（"发布者 'telemetry' 监听传感器 'sht31'"）。所有连接都是类型化指针。

## 相关文档

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — 创建和组装顺序。
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — 架构图。
- [04-patterns/01-add-sensor.md](01-add-sensor.md)、[02-add-peripheral.md](02-add-peripheral.md)、[03-add-transport.md](03-add-transport.md) — 具体组件配方。
