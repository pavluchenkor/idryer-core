# 为新设备添加仪表板卡片

iDryer 生态中的“widget”是**门户仪表板上的设备卡片**，也就是面向具体产品的 React 组件，用可复用模块组合出设备界面。这里没有 widget registry，没有生成的 React 文件，也没有 `contracts/widgets/` 目录。

本页说明如何为新的设备类型添加这种卡片。只做固件时请看 [01-add-new-product.md](01-add-new-product.md)。

---

## 内容放在哪里

| 层 | 仓库 | 事实来源 |
|---|---|---|
| 合约（deviceType、capabilities、invoke actions、telemetry、menu） | `idryer-core` | `contracts/mqtt_contract.yaml` |
| 生成的头文件 / TS 类型 / Dart 类型 | `idryer-core` | `contracts/_generated/*` |
| 固件 | 每个设备自己的仓库（例如 `iHeater-link`、`iDryer-Storage`） | 手写 |
| 仪表板卡片 | `iDryerPortal/frontend-v2` | `src/components/dashboard/cards/*.tsx` |
| 可复用卡片模块 | `iDryerPortal/frontend-v2` | `src/components/device/*.tsx` |
| 卡片注册 | `iDryerPortal/frontend-v2` | `src/components/dashboard/DeviceDashboardCard.tsx` |
| UIKit 预览 | `iDryerPortal/frontend-v2` | `src/pages/UiKitPage.tsx` |

---

## 命令通道规则

卡片通过合约中定义的两条 MQTT 路径与设备通信：

- **Invoke（形式 A）** — 通过菜单动作的 `id` 触发动作，不带参数。适用于动作已经在设备菜单中存在，并且也能从其他客户端触发的情况。
- **Invoke（形式 B）** — 直接发送 `{action, args}`，绕过菜单。适用于带参数的动作（例如带 `tempC`/`durationMin` 的 `heat.start`，或带 `r/g/b/animation` 的 `led.pulse`）。
- **Set** — 写入配置值（`set <role> <value>`）。只用于持久设置，不用于有开始和结束的过程。

对于会开始并结束的过程（加热、干燥、动画），始终使用 **invoke**，不要使用 `set heat_active=true` 这种切换式写法。

---

## 遥测 null 策略

如果传感器不存在或读数不可用，固件必须从遥测 payload 中省略该字段（不要发送 `null`，不要发送 `0`，不要发送 `NaN`）。卡片必须把缺失字段视为“无数据”，显示占位内容，而不是猜测为零。

规范表述见合约中的 `rules.telemetry_null_policy`。

---

## Checklist — 为新设备添加卡片

1. **合约** — 在 `contracts/mqtt_contract.yaml` 中添加设备 profile，以及所有新的 capabilities、roles 和 invoke actions。运行 `./regen.sh` 并提交重新生成的 `_generated/*`。
2. **固件** — 为新动作实现 `onCommand("invoke")`；按 null 策略发送遥测。
3. **卡片组件** — 创建 `iDryerPortal/frontend-v2/src/components/dashboard/cards/<DeviceType>Card.tsx`。使用 [src/components/device/](https://github.com/iDryer/iDryerPortal/tree/main/frontend-v2/src/components/device) 中的可复用模块组合界面。
4. **注册** — 在 [DeviceDashboardCard.tsx](https://github.com/iDryer/iDryerPortal/blob/main/frontend-v2/src/components/dashboard/DeviceDashboardCard.tsx) 的 switch 中添加新的 `deviceType`。
5. **DeviceDetailPage** — 扩展 [DeviceDetailPage.tsx](https://github.com/iDryer/iDryerPortal/blob/main/frontend-v2/src/pages/DeviceDetailPage.tsx) 中的 `controlsOrProgress`，让同一张卡片也显示在设备页面。
6. **UIKit** — 在 [UiKitPage.tsx](https://github.com/iDryer/iDryerPortal/blob/main/frontend-v2/src/pages/UiKitPage.tsx) 的 “Device Widgets” 分组中添加 Idle + Active 示例和 mock 数据，这样无需真实设备也能在 `/uikit` 检查卡片。
7. **测试** — 本地运行门户，确认卡片在 Idle 和 Active 状态下渲染正确，会发送预期的 invoke payload，并会响应遥测更新。
8. **PR** — 在 `idryer-core` 中开一个 PR（合约 + 固件 submodule bump），在 `iDryerPortal` 中开一个 PR（卡片 + 注册 + UIKit）。在描述中互相链接。

---

## 可复用卡片模块

这些模块位于 [src/components/device/](https://github.com/iDryer/iDryerPortal/tree/main/frontend-v2/src/components/device)，应该优先使用。先组合它们，再考虑写自定义 JSX。

| 模块 | 用途 |
|---|---|
| `DeviceHeader` | 设备名称、状态标签、在线/离线指示 |
| `DeviceTelemetryBlock` | 渲染遥测行列表，默认隐藏缺失字段 |
| `ActiveSessionBlock` | 带目标值和剩余时间的过程进度 UI |
| `NumberInput` | 绑定到菜单元数据中的 min/max/step 的数字输入 |
| `CardActions` | 底部按钮组（Start / Stop 等） |

如果新模块会被 2 张或更多卡片复用，请把它放到 `src/components/device/`，不要直接内联在某张卡片里。

---

## 现有卡片（参考）

| 卡片 | 设备类型 | 说明 |
|---|---|---|
| `HeaterCard` | `IHEATER_LINK` | Idle：温度 + 时长输入 + Start。Active：带剩余时间 + Stop 的 ActiveSessionBlock。 |
| `StorageCard` | `STORAGE_LINK` | SHT31 遥测 + LED 动画/颜色选择器 + Turn On/Off（invoke `led.pulse`）。 |
| `IDryerCard` | fallback | 没有专用实现的设备使用通用卡片。 |

开始新卡片前，先打开这些现有卡片作为具体示例。
