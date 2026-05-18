# 配置文件模型

配置文件是 `IProfile` 接口的实现，它描述特定设备的行为。库仅通过该接口与产品交互。

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

### 库调用每个方法的时间

| 方法 | 调用时间 | 它必须做什么 |
|------|---------|-----------|
| `onOnline()` | 在第一次 `CloudStateMachine` 转换到 `Online` 时 | 从 NVS 加载配置，应用到硬件 |
| `loop()` | `IdryerRuntime::loop()` 的每次迭代 | 计时器、动画、传感器轮询 |
| `buildInfoJson(buf, len)` | 转换到在线时；在 `ping` 时 | 序列化设备信息有效负载 |
| `getConfig(out)` | 在 `invoke device.getConfig` 时 | 用当前配置填充文档 |
| `applyConfig(id, val)` | 在 `commands/set` 时 | 应用参数，保存到 NVS |

## 示例：LedStripProfile

`LedStripProfile` 是存储链接的配置文件实现。位于 `src/storage/led_strip/` 中。

```cpp
class LedStripProfile : public IProfile {
public:
    explicit LedStripProfile(LedStripExecutor* executor);

    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;

    static void normalizeGroups();        // 修复切换组的 NVS 状态
    static uint8_t selectedStripType();   // 0=WS2812B, 1=APA102
    static uint8_t selectedColorOrder();  // 0=GRB, 1=RGB, 2=BRG, 3=BGR

    static constexpr const char* DEVICE_TYPE = "storage_link";
    static constexpr const char* HW_VERSION  = "1.0";
    static constexpr const char* FW_VERSION  = "1.0.0";

private:
    LedStripExecutor* executor_;
};
```

`onOnline()` 应用当前 LED 条配置（LED 计数、亮度）到 `LedStripExecutor`。

`applyConfig(id, val)` 接受来自 `menu_ids.h` 的参数 ID 和新值。通过 `menu` 对象保存到 NVS。诸如 `strip_type` 和 `color_order` 之类的参数需要重启——FastLED 在启动时初始化一次。

`buildInfoJson` 构建 `idryer/{serial}/info` 的有效负载。字段组成由产品定义。存储链接发布：

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

对于具有多个舱室单元的设备（iDryer LINK），添加 `workTimeCounter`、`unitsCount` 和描述功能的 `units` 数组是典型的。

## ActionDispatcher

`ActionDispatcher` 路由两种命令类型而不使用 std::function（普通函数指针以节省堆）：

```cpp
// Invoke：具有名称和参数的操作
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set：设置单个参数
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

在 `setup()` 中注册：

```cpp
// Invoke — 委托给 LedStripExecutor
dispatcher.setInvokeHandler(
    [](const char* action, JsonObjectConst args, void* /*ctx*/) -> bool {
        return s_executor.execute(action, args);
    }, nullptr);

// Set — 将 id/val 传递给 LedStripProfile
dispatcher.setSetCallback(
    [](JsonObjectConst data, void* /*ctx*/) {
        int id  = data["id"]  | -1;
        int val = data["val"] | -1;
        s_profile.applyConfig(id, val);
    }, nullptr);
```

当相应的 MQTT 命令到达时，`IdryerRuntime` 调用 `dispatcher.handleInvoke(data)` 和 `dispatcher.handleSet(data)`。

## 创建新配置文件

1. 创建一个继承自 `IProfile` 的类。
2. 实现所有五个方法。
3. 将指向配置文件的指针传递到 `IdryerRuntime` 构造函数中。
4. 在 `ActionDispatcher` 中为 `invoke` 和 `set` 命令注册处理程序。

对于配置文件在其方法内做什么没有限制——它对产品上下文具有完全的可见性。
