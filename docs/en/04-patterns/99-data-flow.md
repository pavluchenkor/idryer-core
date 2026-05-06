# Data flow between participants

Applied section: how sensors, peripherals, profile, transports, and publishers are connected in real product code. The architectural data flow description is in [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md).

## Principle

`idryer-core` deliberately does not provide an internal event bus. All connections between participants are **explicit pointers** passed through constructors in the composition root. This means:

- Any data flow can be read as a chain of pointers in `main.cpp`.
- No "magic" participant discovery.
- The product decides who passes what to whom.

## Typical connection map for Storage Link

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

Each arrow is one pointer-passing line in `main.cpp`. For example:

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     publisher
```

## Recipe 1 — Sensor publishes to the cloud

**Goal**: temperature sensor → MQTT.

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

`MyTelemetryPublisher::loop` decides when to publish (by interval). See [01-add-sensor.md](01-add-sensor.md).

## Recipe 2 — Cloud command → peripheral

**Goal**: `commands/invoke {"action":"led.pulse",...}` → turn on LED.

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

See [02-add-peripheral.md](02-add-peripheral.md).

## Recipe 3 — LAN app command → peripheral (same path)

**Goal**: WS client on LAN sends `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → the same LED turns on.

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

No new code needed — `s_local.setCommandSink(handleCommand)` already merges both transports into one handler.

## Recipe 4 — Sensor → Peripheral (internal loop)

**Goal**: sensor reads humidity → if above threshold, fan turns on.

This is internal product logic; `idryer-core` has no API for such connections. Do it directly:

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

Connecting in the composition root:

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` knows nothing about this class and should not.

## Recipe 5 — Config change → peripheral reinitialization

**Goal**: backend sends `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → LED brightness changes immediately.

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

The `profile → peripheral` connection is built in the composition root:

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## Recipe 6 — New event → events topic

**Goal**: peripheral catches an error → event in `idryer/{serial}/events`.

The peripheral does not publish on its own. It notifies the product; the product publishes:

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

Alternatively, the peripheral can accept a `DevicePublisher*` through its constructor. The key point: the connection is explicit.

## What we do not do

- We do not introduce an internal event bus. This would lead to hidden connections and debugging complexity.
- We do not collect sensor/peripheral/publisher into a shared `IDeviceContainer`. Connections are built precisely in the composition root.
- We do not use name-based subscriptions ("publisher 'telemetry' listens to sensor 'sht31'"). All connections are typed pointers.

## Related documents

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — creation and assembly order.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — architectural diagram.
- [04-patterns/01-add-sensor.md](01-add-sensor.md), [02-add-peripheral.md](02-add-peripheral.md), [03-add-transport.md](03-add-transport.md) — concrete component recipes.
