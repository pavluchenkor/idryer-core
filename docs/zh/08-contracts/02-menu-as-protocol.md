# 作为协议的菜单：menu.yaml ↔ mqtt_contract.yaml ↔ 门户

---

## 三个文件，三种角色

| 文件 | 所有者 | 描述内容 |
|------|-------|-----------|
| `src/menu/menu.yaml` | 你的产品 | 设备菜单：参数、动作、结构 |
| `contracts/mqtt_contract.yaml` | idryer-core | 已知含义列表：带多语言标签的 `canonical_roles` |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | 生成 | 门户用的 TypeScript 类型 |

**`role:`** —— 菜单项的语义名称。固件说“我有 `iheater.heat_temp`”，而不是“我有第 35 项”。固件内部名称可以变，`role:` 保持不变。

菜单是设备设置的镜像。固件发布 `menu.yaml` 中的所有项，无论有没有 `role:`。门户按每一项的类型绘制：数值、开关、动作、子菜单。`role:` 为该项提供合同中用户语言的标签；没有 `role:` 时，门户显示设备发来的名称。

设备卡片不是由菜单构建的。卡片显示什么、启动哪些操作，由 card 清单描述——见 [设备卡片：card 清单](../09-add-product/02-add-widget.md)。卡片动作的参数可以从菜单项获取范围和默认值。

---

## 1. 固件构建（`pio run`）

`menu.yaml` → `menu_gen.py` 将每个 `role:` 与合同中的 `canonical_roles` 核对 → 如有未知角色，构建失败并列出有效角色 → 生成器把 C++ 文件写入 `src/menu/`。

## 2. 为门户更新 TypeScript（`regen.sh`）

`mqtt_contract.yaml` → `gen_ts_types.py` 生成 `mqtt-api.types.ts` 和角色标签 `roles.{lang}.json` → 文件被复制到门户。

合同变更时运行，并提交结果。

## 3. 运行时：设备 ↔ 门户

固件把菜单发布到 `config` 主题（由产品代码在收到 `get_config` 命令时完成；iDryer 产品在上线时也会发布）→ 门户后端保存 → 门户通过 `GET /devices/:id/menu-config` 获取 → 每一项按类型 `t`（`val`、`tog`、`act`、`sub`）绘制，标签依次取 `canonical_roles[r].labels[lang]`、英文、设备发来的名称 `n`。

参数（`min`、`max`、`val`）来自菜单项本身——当前值由固件掌握。

门户通过 `commands/set { "id": <id>, "val": <value> }` 修改数值。

---

## 如何添加设置（NVS 参数）

```yaml
- id: my_param
  type: value
  role: my.param        # 可选：来自合同的标签
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS 键（≤ 15 个字符）
  persist: true
  scope: global
  default: 50
```

`bind` = NVS 键。`persist: true` = 重启后值仍保留。

`role:` 不是自由字段：值必须来自合同中的 `canonical_roles`，否则构建失败。列表见 `contracts/mqtt_contract.yaml` → `canonical_roles` 或 `menu.template.yaml`。新角色先加入合同，再运行 `regen.sh`。

---

## 如何在设备卡片上添加操作

操作（启动、停止、加热、照明）声明为卡片动作，而不是菜单项。其范围可以取自菜单项：

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

完整说明 —— [设备卡片：card 清单](../09-add-product/02-add-widget.md)。

---

## 不要做的事

- 不要在 `menu.yaml` 中添加 `widget:`。`canonical_roles` 的 `widget` 字段仅供参考：门户和应用都不读取它。
- 不要手动编辑 `mqtt-api.types.ts` —— 它由 `regen.sh` 生成。
- 不要为新动作改动 `Config.hasXxx` 标志 —— 它们只用于遥测（传感器、状态）。
