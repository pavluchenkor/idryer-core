# 添加小部件和新设备

完整周期：从 fork 存储库到合并 PR。涵盖固件、合约、React 小部件和门户测试。

如果你只需要没有新小部件的固件 — 参看 [01-add-new-product.md](01-add-new-product.md)。

---

## 前置条件

- Python 3.9+ 并 `pip install pyyaml jsonschema`
- Node.js 18+
- PlatformIO CLI
- 访问 iDryer 门户以进行 UIKit 测试

---

## 第 1 步。Fork 和 Clone

1. 在 GitHub 上 fork `idryer-core` 存储库。
2. 在本地 clone 你的 fork：

    ```bash
    git clone https://github.com/<your-username>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. 验证合约在当前状态下通过验证：

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## 第 2 步。编辑合约

所有更改进入 `contracts/mqtt_contract.yaml`。保持一个单独的变更集中的所有内容。

!!! warning
    不要编辑 `_generated/` 中的文件 — 它们由生成器覆盖。

### 2a. 能力词汇(新外设类型)

如果设备具有新的硬件类型(例如，CO2 传感器)，向 `capability_vocabulary` 部分添加条目：

```yaml
capability_vocabulary:
  co2:
    description: "CO2 sensor (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

这自动在下一次重生时向 `iDryer::Config` 添加字段 `hasAirCo2: bool`。

### 2b. 规范角色(新角色 + 小部件)

如果设备公开新菜单项，在 `canonical_roles` 中注册角色：

```yaml
canonical_roles:
  co2.read:
    type: float
    widget: Co2Display
    unit: ppm
    labels:
      ru: "CO₂"
      en: "CO₂"
```

`widget` 值是你将在第 5 步中编写的 React 组件的名称。

### 2c. Invoke 动作(如果小部件发送命令)

如果小部件在设备上触发动作，在 `invoke_actions` 中描述它：

```yaml
invoke_actions:
  my_device:
    co2.calibrate:
      description: "Start CO2 sensor calibration"
      args:
        targetPpm:
          type: uint16
          description: "Reference CO2 value (ppm)"
          required: true
```

### 2d. 设备个人资料(新设备类型)

将个人资料添加到 `device_profiles`：

```yaml
device_profiles:
  my_device:
    description: "My device"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

能力值来自第 2a 步中定义的 `capability_vocabulary`。

---

## 第 3 步。验证和重生

```bash
cd contracts
./regen.sh
```

标志：

| 标志 | 效果 |
|------|------|
| (none) | 验证 + 所有生成器 + 复制到门户 |
| `--firmware-only` | 仅固件生成器，跳过门户复制 |
| `--help` | 显示帮助 |

成功时，`_generated/` 使用以下内容更新：

- `uart_protocol.h`、`mqtt_topics.h` — C++ 头文件
- `iDryer_api.h` — Config/DeviceType 外观
- `mqtt-api.types.ts` — TypeScript 类型
- `scaffolds/my_device/` — PlatformIO 项目骨架
- 在门户上：`src/components/widgets/` 中的文件

如果 `regen.sh` 以错误退出，在继续前修复问题。

---

## 第 4 步。实现固件

使用生成的骨架项目：

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

填充 `src/main.cpp` 中的 TODO 部分：

- `onOnline()` — 从 NVS 加载配置，初始化硬件。
- `loop()` — 轮询传感器，调用 `s_runtime.publishTelemetry(tel)`。
- `buildInfoJson()` — 已由生成器从能力填充。
- `onInvoke()` — 处理 `co2.calibrate`。

有关详细信息，参看 [01-add-new-product.md](01-add-new-product.md)。

---

## 第 5 步。创建 React 小部件

小部件位于 `contracts/widgets/` 中，由 `regen.sh` 复制到门户。

!!! note
    不要直接在 `portal/src/components/widgets/` 中编辑小部件 — 它们将在下一次 `regen.sh` 运行中被覆盖。仅在 `contracts/widgets/` 中编辑。

### 创建小部件文件

```tsx
// contracts/widgets/Co2Display.tsx
import type { WidgetProps } from "./widget-props";

export function Co2DisplayWidget({ device }: WidgetProps) {
  const unit = device.units[0];
  const co2 = unit?.co2Ppm ?? null;
  return (
    <div style={{ padding: "8px 16px" }}>
      {co2 !== null ? `${co2} ppm` : "—"}
    </div>
  );
}
```

### 在 index.ts 中注册

```ts
// contracts/widgets/index.ts
export { Co2DisplayWidget } from "./Co2Display";
```

### 在 widget-registry.tsx 中注册(在门户上)

下一次 `regen.sh` 运行后，文件将出现在 `portal/src/components/widgets/Co2Display.tsx`。手动向 `widget-registry.tsx` 添加条目：

```tsx
import { Co2DisplayWidget } from "./Co2Display";

export const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  // ...
  Co2Display: Co2DisplayWidget,
};
```

---

## 第 6 步。在 UIKit 中测试

打开 `portal/src/pages/UiKitPage.tsx`，在**Device Dashboard Widgets** 组中添加带有模拟数据的部分：

```tsx
<KitSection title="Co2Display">
  <Co2DisplayWidget device={MOCK_DEVICE} item={MOCK_CO2_ITEM} socket={null} />
</KitSection>
```

在本地打开门户并导航到 `/uikit` — 小部件应该无需登录即可呈现。

---

## 第 7 步。PR 检查清单

在提交 PR 前，验证：

- [ ] `./contracts/regen.sh` 完成无错误
- [ ] `_generated/*` 已提交(不在 `.gitignore` 中)
- [ ] `contracts/widgets/` — 添加了新小部件文件
- [ ] `contracts/widgets/index.ts` — 小部件已导出
- [ ] 门户上的 `widget-registry.tsx` — 小部件已注册
- [ ] 小部件在 `/uikit` 呈现无控制台错误
- [ ] `_generated/scaffolds/my_device/` 中的骨架正确反映能力
- [ ] PR 描述说明：设备目的、能力、小部件名称

针对 `idryer-core` 存储库的 `main` 分支提交 PR。

---

## 一个 PR 中的所有更改

| 文件 | 更改类型 |
|------|---------|
| `contracts/mqtt_contract.yaml` | 真实来源 |
| `contracts/_generated/*` | 自动生成 — 完全提交 |
| `contracts/widgets/MyWidget.tsx` | 新文件 |
| `contracts/widgets/index.ts` | +1 导出行 |
| *(门户，在 `regen.sh` 之后)* | `src/components/widgets/MyWidget.tsx` — 复制 |
| *(门户，手动)* | `src/components/widgets/widget-registry.tsx` — +1 条目 |
| *(门户，手动)* | `src/pages/UiKitPage.tsx` — KitGroup 中 +1 部分 |
