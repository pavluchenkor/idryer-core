# 菜单作为协议：menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## 三个文件 — 三个角色

| 文件 | 所有者 | 描述 |
|------|--------|------|
| `src/menu/menu.yaml` | 你的产品 | 设备菜单：参数、动作、结构 |
| `contracts/mqtt_contract.yaml` | idryer-core | 已知含义的列表：每个 `role:` 是什么意思以及门户如何显示它 |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | 生成 | 门户的 TypeScript 类型 |

**`role:`** — 菜单项的语义名称。固件说"我有 `iheater.heat_start`"而不是"我有按钮号 35"。这是设备和门户之间的稳定合约 — 内部固件名称可以更改，`role:` 保持固定。

**小部件** — 门户如何显示此项：按钮、滑块、切换开关或复杂组件(色彩选择器、个人资料编辑器)。由合约通过 `role:` 确定，不由固件确定。

带有 `role:` 的菜单项对门户可见。没有 `role:` — 私有，仅在设备显示屏上显示。

---

## 1. 固件构建(`pio run`)

`menu.yaml` → `pre_gen_menu.py` 验证每个 `role:` 与合约中的 `canonical_roles` → 如果角色未知，构建失败并显示错误和有效角色列表 → `menu_gen.py` 生成 C++ 文件到 `src/menu/`

验证内置于构建步骤中 — 不可能无声地使用不存在的角色。

## 2. 为门户更新 TypeScript(`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` 生成 `mqtt-api.types.ts` → 文件复制到 `frontend-v2/src/contracts/`

合约更改时手动运行。提交结果。

## 3. 运行时：设备 ↔ 门户

设备连接 → 发布菜单到 MQTT 主题 `config` → 门户读取每个带有字段 `r:` 的项 → 查找 `CanonicalRoles[r].widget` → 从 `WIDGET_REGISTRY` 呈现小部件。

参数(`min`、`max`、`val`)来自菜单项本身 — 固件知道当前值。

---

## 如何向门户仪表板添加新动作

`role:` 不是自由格式的字段。该值必须来自合约中 `canonical_roles` 的闭合列表。你不能即时发明角色 — 构建会失败。查看 `contracts/mqtt_contract.yaml` → `canonical_roles` 部分中的可用角色，或在 `menu.template.yaml` 中。

**1. 从合约中选择角色。** 如果没有合适的 — 首先将其添加到 `mqtt_contract.yaml` → `canonical_roles`，然后运行 `regen.sh`：

```yaml
canonical_roles:
  my.action: { type: action, widget: button }
```

**2. 向 `menu.yaml` 添加项：**

```yaml
- id: my_action
  type: action
  role: my.action
  title: { ru: "МОЁ ДЕЙСТВИЕ", en: "MY ACTION" }
```

**3. 在固件中处理它(`main.cpp`)：**

```cpp
if (action == "my.action") { /* do the thing */ }
```

`pio run` → 验证 → C++ → 固件发布 `r: "my.action"` → 门户呈现按钮。

---

## 如何添加设置(NVS 参数)

```yaml
- id: my_param
  type: value
  role: my.param        # 仅当它应该出现在门户上；对于仅显示则省略
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS 密钥(≤ 15 个字符)
  persist: true
  scope: global
  default: 50
```

`bind` = NVS 密钥。`persist: true` = 值在重启后保留。

门户通过 `commands/set { "id": <id>, "val": <value> }` 更改值。

---

## 不要做什么

- 不要向 `menu.yaml` 添加 `widget:` — 小部件由合约通过 `role:` 确定，不由固件确定
- 不要手动编辑 `mqtt-api.types.ts` — 它由 `regen.sh` 生成
- 不要为新动作触及 `Config.hasXxx` 标志 — 那些仅用于遥测(传感器、状态)
