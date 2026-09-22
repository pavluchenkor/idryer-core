---
title: "设备卡片：card 清单"
description: "基于 idryer-core 的固件如何用 s_link.card() 描述自己的卡片：传感器、带启动参数的动作、发送到门户的内容，以及门户和应用绘制的内容。"
---

# 设备卡片：card 清单

门户和移动应用根据设备的 **card 清单**（card manifest）构建任何设备的卡片。清单是固件对自身的描述：显示什么、可以控制什么。新的设备类型不需要在门户或应用中编写代码。

清单属于 `iDryer::Link` 外观类（`s_link.card()`）。底层运行时（`IdryerRuntime`）不会发布清单。

菜单和卡片是两回事。菜单是设备设置的镜像：在门户上修改的值会写入设备存储。卡片显示测量值并启动操作：启动参数随一条命令发送，不会写入菜单。启动参数可以从菜单项获取范围和默认值。

---

## 工作原理

```text
固件：s_link.card() 声明
   │  核心库生成 JSON，以 retained（QoS 1）发布到 idryer/{key}/card
   ▼
门户后端：校验清单（上限、允许的类型和字段）并保存
   ▼
门户和应用：绘制卡片
   │  用户按下按钮
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
核心库把 "card.<id>" 交给你的回调
```

MQTT 连接建立后，核心库发布清单；当声明或其依赖的菜单项发生变化时，重新发布。

---

## 实体：显示什么

生态词汇表中的传感器根据 `Config` 标志自动加入，无需声明：

| `Config` 标志 | 卡片单元格 |
|---|---|
| `hasAirTemp` | 空气温度 |
| `hasAirHumidity` | 湿度 |
| `hasHeaterTemp` | 加热器温度 |
| `hasHeater` | 加热功率 |
| `hasFan` | 风扇 开 / 关 |
| `hasServo` | 风门 开 / 关 |
| `hasWeight` | 称重模块（topic `weights`） |
| `hasRfid` | 该单元有 RFID 读卡器 |

自定义数值：写入遥测，并声明带 JSON 路径的传感器：

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

简单控件也是实体：`button`、`number`、`select`。用户一改值就会发送，核心库调用你的回调：

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## 动作：带启动参数的操作

动作是设备的操作：开始烘干、加热、开灯、停止。动作有一个 **模式**——动作之后单元的模式（`status.units[].mode`）——以及带有含义（`purpose`）的 **参数**。卡片根据单元当前的模式决定显示什么：

- 单元的模式与某个动作的模式一致 → 设备正在执行它：显示会话块和停止按钮（模式为 `IDLE` 的动作）；
- 否则 → 显示启动表单；有多个启动动作时 → 显示模式切换。

示例：用一块 ESP32 制作的加热储料柜。目标温度是菜单项 `target_temp`（30–50 °C，默认 45）。

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // 已在 30..50 范围内
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // 把菜单值写入卡片读取的缓存
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

回调收到的内容：

- 数值已被限制在单元的范围内，缺失的数值用默认值代替；
- `unit` —— 由 `unitId` 得到的单元索引（`"U1"` → 0）；发给设备不存在的单元的命令会被忽略；
- 启动后设置 `status.mode` 并调用 `publishStatusNow()`——卡片根据状态切换到会话块。

### 动作 API

| 调用 | 作用 |
|---|---|
| `card.action(id, mode, cb)` | 一个动作；`mode` —— 执行后单元的模式，`nullptr` —— 模式不变 |
| `.param(id, purpose, MENU_ID)` | 数值：范围、步长、默认值和单位取自菜单项 |
| `.param(id, purpose, min, max, step, def[, unit])` | 自带范围的数值 |
| `.ceiling(MENU_ID)` | 前一个参数的上限取该菜单项的值（例如最高空气温度） |
| `.stages(id, "stages", MENU_ID)` | 曲线阶段 `[{temperature, ramp, hold}]`，单位秒；阶段温度在菜单项范围内 |
| `.select(id, purpose, options, count[, def])` | 从列表中选择；列表外的值用默认值代替 |
| `.color(id, purpose, "#FFFFFF")` | 颜色 `#RRGGBB` |
| `.deviceClass("identify")`、`.deviceClass("clear_errors")` | 卡片头部的常驻按钮：定位设备、清除错误 |
| `.name("ru", "…").name("en", "…")` | 卡片不认识该模式时显示的动作名称 |

`purpose` 取值：`target_temperature`、`target_humidity`、`duration`（分钟，0 —— 不限时）、`stages`、`start_stage`（从 0 开始）、`effect`、`rgb_color`。卡片用它们给字段加标签并接入门户功能：烘干预设填写模式为 `DRYING` 的动作的 `target_temperature` 和 `duration`，烘干曲线填写 `stages`。

回调是函数或无捕获的 lambda。`name` 字符串和选项以指针形式保存——请使用字面量或静态数组。

---

## 发送到门户的内容

上面的储料柜发布如下内容（`Config`：`hasAirTemp`、`hasAirHumidity`、`hasHeaterTemp`、`hasHeater`、`hasFan`）：

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` 和 `default` 来自菜单项。使用 `.ceiling()` 时，参数还会带有 `max_by`——压低上限的那个菜单项的标题；数值超过上限时，卡片会指出它。

---

## 门户和应用绘制的内容

示意图，不是截图。空闲时：

```text
┌─ DIY Storage Cabinet ──────────────── [Idle] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                 │  ← 温度、湿度、加热器
│ | 0 %     | off             |                 │  ← 功率、风扇（灰色：0 / 关）
│ [Temp. 45 °C          ]  [Storage]            │  ← 动作 "storage"
└───────────────────────────────────────────────┘
```

启动后，设备报告模式 `STORAGE`，同一张卡片显示会话块（目标温度和已存放时间）和停止按钮——模式为 `IDLE` 的动作。

两端共同的规则：

- 没有数值的单元格显示为灰色；功率为 0 时也是灰色；风扇和风门在关闭状态时为灰色；
- 超出范围的值不会被替换：启动被阻止，表单下方显示原因；
- 对于清单中有动作的固件，只有声明了 `identify` / `clear_errors` 动作，才会出现定位和清除错误按钮。

卡片出现的位置：

| | 门户 | 应用 |
|---|---|---|
| 非 iDryer 产品的设备类型 | 仪表板上按清单生成的卡片，带动作；设备页面 —— 读数和动作（清单有动作时） | 首页 —— 读数；动作 —— 在设备页面 |
| `deviceType = Dryer` | 带同样清单动作的烘干机卡片 | 首页 —— 监控；动作 —— 在设备页面 |

---

## 上限

| | 核心库 | 门户后端 |
|---|---|---|
| 实体 | 声明 16 个 + 自动 | 32 |
| layout 行数 | 8 | 16 |
| 每行 id 数 | 4 | 4 |
| 动作 | 8 | 8 |
| 每个动作的参数 | 4（`IDRYER_CARD_MAX_PARAMS`） | 6 |
| `name` 语言 | `ru`、`en` | 最多 4 |

`id`：小写拉丁字母、数字和 `_`，最多 24 个字符；实体和动作共用一个命名空间。清单文档上限为 4096 字节；放不下时，核心库在日志中输出 `manifest overflows`，并且不发布。

格式本身 —— `contracts/mqtt_contract.yaml`，`mqtt_only` 部分，`suffix: card`。
