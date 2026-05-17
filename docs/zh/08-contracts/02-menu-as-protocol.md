# 協議形式的菜單: menu.yaml ↔ mqtt_contract.yaml ↔ 門戶

---

## 三個文件 — 三個角色

| 文件 | 所有者 | 描述 |
|------|-------|-----------|
| `src/menu/menu.yaml` | 你的產品 | 設備菜單: 參數、操作、結構 |
| `contracts/mqtt_contract.yaml` | idryer-core | 已知含義的列表: 每個 `role:` 的含義以及門戶如何顯示它 |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | 生成 | 門戶的 TypeScript 類型 |

**`role:`** — 菜單項的語義名稱。韌體說 "I have `iheater.heat_start`" 而不是 "I have button number 35"。這是設備和門戶之間的穩定合約 — 內部韌體名稱可以改變，`role:` 保持固定。

**小部件** — 門戶如何顯示此項: 按鈕、滑塊、開關或複雜的組件 (調色板、配置文件編輯器)。通過 `role:` 由合約決定，而不是由韌體決定。

帶有 `role:` 的菜單項對門戶可見。沒有 `role:` — 私有，僅在設備顯示屏上顯示。

---

## 1. 韌體構建 (`pio run`)

`menu.yaml` → `pre_gen_menu.py` 根據合約中的 `canonical_roles` 驗證每個 `role:` → 如果角色未知，構建失敗並出現錯誤和有效角色列表 → `menu_gen.py` 生成 C++ 文件到 `src/menu/`

驗證內置在構建步驟中 — 不可能無聲地使用不存在的角色。

## 2. 更新門戶的 TypeScript (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` 生成 `mqtt-api.types.ts` → 文件被複製到 `frontend-v2/src/contracts/`

在合約更改時手動運行。提交結果。

## 3. 運行時: 設備 ↔ 門戶

設備連接 → 發佈菜單到 MQTT 主題 `config` → 門戶讀取帶有字段 `r:` 的每一項 → 查找 `CanonicalRoles[r].widget` → 從 `WIDGET_REGISTRY` 渲染小部件。

參數 (`min`, `max`, `val`) 來自菜單項本身 — 韌體知道當前值。

---

## 如何向門戶儀表板添加新操作

`role:` 不是自由形式字段。該值必須來自合約中 `canonical_roles` 的閉合列表。你不能臨時發明角色 — 構建將失敗。請參閱 `contracts/mqtt_contract.yaml` → `canonical_roles` 部分或 `menu.template.yaml` 中的可用角色。

**1. 從合約中選擇一個角色。** 如果沒有合適的 — 首先將其添加到 `mqtt_contract.yaml` → `canonical_roles`，然後運行 `regen.sh`:

```yaml
canonical_roles:
  my.action: { type: action, widget: button }
```

**2. 向 `menu.yaml` 添加一個項:**

```yaml
- id: my_action
  type: action
  role: my.action
  title: { ru: "МОЁ ДЕЙСТВИЕ", en: "MY ACTION" }
```

**3. 在韌體中處理它 (`main.cpp`)**:

```cpp
if (action == "my.action") { /* do the thing */ }
```

`pio run` → 驗證 → C++ → 韌體發佈 `r: "my.action"` → 門戶呈現按鈕。

---

## 如何添加設置 (NVS 參數)

```yaml
- id: my_param
  type: value
  role: my.param        # 僅當它應在門戶上顯示時; 對於僅顯示則省略
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS 鍵 (≤ 15 個字符)
  persist: true
  scope: global
  default: 50
```

`bind` = NVS 鍵。`persist: true` = 值在重啟後仍然存在。
門戶通過 `commands/set { "id": <id>, "val": <value> }` 改變值。

---

## 不要做什麼

- 不要向 `menu.yaml` 添加 `widget:` — 小部件由合約通過 `role:` 決定，而不是由韌體決定
- 不要手動編輯 `mqtt-api.types.ts` — 它由 `regen.sh` 生成
- 不要觸碰新操作的 `Config.hasXxx` 標誌 — 這些僅用於遙測 (傳感器、狀態)
