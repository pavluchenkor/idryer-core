# 設置文件模型

A profile is an implementation of the `IProfile` interface, which describes the behaviour of a specific device. The library interacts with the product only through this interface.

## IProfile 接口

```cpp
class IProfile {
public:
    virtual ~IProfile() = default;

    virtual void onOnline() = 0;
    virtual void loop() = 0;
    virtual void getConfig(JsonDocument& out) = 0;
    virtual bool applyConfig(int id, int val) = 0;
    virtual void buildInfoJson(char* buf, size_t len) const = 0;
};
```

### 庫何時調用每個方法

| Method | When called | What it must do |
|--------|------------|----------------|
| `onOnline()` | On the first `CloudStateMachine` transition to `Online` | Load config from NVS, apply to hardware |
| `loop()` | Every iteration of `IdryerRuntime::loop()` | Timers, animations, sensor polling |
| `buildInfoJson(buf, len)` | On transition to Online; on `ping` | Serialize device info payload |
| `getConfig(out)` | On `invoke device.getConfig` | Fill doc with current config |
| `applyConfig(id, val)` | On `commands/set` | Apply parameter, save to NVS |

## 示例：LedStripProfile

`LedStripProfile` is the profile implementation for Storage Link. Located in `src/storage/led_strip/`.

```cpp
class LedStripProfile : public IProfile {
public:
    explicit LedStripProfile(LedStripExecutor* executor);

    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;

    static void normalizeGroups();        // fix NVS state of toggle groups
    static uint8_t selectedStripType();   // 0=WS2812B, 1=APA102
    static uint8_t selectedColorOrder();  // 0=GRB, 1=RGB, 2=BRG, 3=BGR

    static constexpr const char* DEVICE_TYPE = "storage_link";
    static constexpr const char* HW_VERSION  = "1.0";
    static constexpr const char* FW_VERSION  = "1.0.0";

private:
    LedStripExecutor* executor_;
};
```

`onOnline()` applies the current LED strip configuration (LED count, brightness) to `LedStripExecutor`.

`applyConfig(id, val)` accepts a parameter ID from `menu_ids.h` and a new value. Saves to NVS via the `menu` object. Parameters such as `strip_type` and `color_order` require a reboot — FastLED is initialized once at startup.

`buildInfoJson` builds the payload for `idryer/{serial}/info`. Field composition is defined by the product. Storage Link publishes:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

For devices with multiple chamber units (iDryer LINK), it is typical to add `workTimeCounter`, `unitsCount`, and a `units` array describing capabilities.

## ActionDispatcher

`ActionDispatcher` routes two command types without std::function (plain function pointers to conserve heap):

```cpp
// Invoke: action with name and arguments
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: setting a single parameter
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

Registration in `setup()`:

```cpp
// Invoke — delegates to LedStripExecutor
dispatcher.setInvokeHandler(
    [](const char* action, JsonObjectConst args, void* /*ctx*/) -> bool {
        return s_executor.execute(action, args);
    }, nullptr);

// Set — passes id/val to LedStripProfile
dispatcher.setSetCallback(
    [](JsonObjectConst data, void* /*ctx*/) {
        int id  = data["id"]  | -1;
        int val = data["val"] | -1;
        s_profile.applyConfig(id, val);
    }, nullptr);
```

`IdryerRuntime` calls `dispatcher.handleInvoke(data)` and `dispatcher.handleSet(data)` when the corresponding MQTT commands arrive.

## 創建新設置文件

1. Create a class inheriting from `IProfile`.
2. Implement all five methods.
3. Pass a pointer to the profile into the `IdryerRuntime` constructor.
4. Register handlers in `ActionDispatcher` for `invoke` and `set` commands.

There are no restrictions on what the profile does inside its methods — it has full visibility into the product context.
