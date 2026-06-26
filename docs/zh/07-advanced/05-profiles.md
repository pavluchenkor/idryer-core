# Profile 模型

Profile 是 `IProfile` 接口的实现，用来描述某个具体设备的行为。库只通过这个接口与产品交互。

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

### 库何时调用每个方法

| 方法 | 何时调用 | 必须做什么 |
|--------|------------|----------------|
| `onOnline()` | 第一次从 `CloudStateMachine` 进入 `Online` 时 | 从 NVS 加载配置并应用到硬件 |
| `loop()` | 每次 `IdryerRuntime::loop()` 迭代 | 定时器、动画、传感器轮询 |
| `buildInfoJson(buf, len)` | 进入 Online 时；收到 `ping` 时 | 序列化设备 info payload |
| `getConfig(out)` | 收到 `invoke device.getConfig` 时 | 用当前配置填充 doc |
| `applyConfig(id, val)` | 收到 `commands/set` 时 | 应用参数并保存到 NVS |

## 示例：LedStripProfile

`LedStripProfile` 是 Storage Link 的 profile 实现。位于 `src/storage/led_strip/`。

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

`onOnline()` 会把当前 LED 灯带配置（LED 数量、亮度）应用到 `LedStripExecutor`。

`applyConfig(id, val)` 接收来自 `menu_ids.h` 的参数 ID 和新值。通过 `menu` 对象保存到 NVS。`strip_type` 和 `color_order` 等参数需要重启，因为 FastLED 只在启动时初始化一次。

`buildInfoJson` 构建 `idryer/{serial}/info` 的 payload。字段组成由产品定义。Storage Link 发布：

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

对于包含多个腔体单元的设备（iDryer LINK），通常会添加 `workTimeCounter`、`unitsCount`，以及描述能力的 `units` 数组。

## ActionDispatcher

`ActionDispatcher` 路由两种命令类型，且不使用 std::function（使用普通函数指针以节省 heap）：

```cpp
// Invoke: action with name and arguments
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: setting a single parameter
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

在 `setup()` 中注册：

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

当对应的 MQTT 命令到达时，`IdryerRuntime` 会调用 `dispatcher.handleInvoke(data)` 和 `dispatcher.handleSet(data)`。

## 创建新 profile

1. 创建一个继承自 `IProfile` 的类。
2. 实现全部五个方法。
3. 将 profile 指针传入 `IdryerRuntime` 构造函数。
4. 在 `ActionDispatcher` 中为 `invoke` 和 `set` 命令注册 handler。

Profile 在自己的方法内部做什么没有限制；它可以完全访问产品上下文。
