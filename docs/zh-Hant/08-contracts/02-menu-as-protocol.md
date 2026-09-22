# 作為協定的選單：menu.yaml ↔ mqtt_contract.yaml ↔ 門戶

---

## 三個檔案，三種角色

| 檔案 | 擁有者 | 描述內容 |
|------|-------|-----------|
| `src/menu/menu.yaml` | 你的產品 | 裝置選單：參數、動作、結構 |
| `contracts/mqtt_contract.yaml` | idryer-core | 已知含義清單：帶多語言標籤的 `canonical_roles` |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | 產生 | 門戶用的 TypeScript 型別 |

**`role:`** —— 選單項目的語義名稱。韌體說「我有 `iheater.heat_temp`」，而不是「我有第 35 項」。韌體內部名稱可以改變，`role:` 保持不變。

選單是裝置設定的鏡像。韌體發布 `menu.yaml` 中的所有項目，無論有沒有 `role:`。門戶依每一項的類型繪製：數值、開關、動作、子選單。`role:` 為該項提供合約中使用者語言的標籤；沒有 `role:` 時，門戶顯示裝置傳來的名稱。

裝置卡片不是由選單建立的。卡片顯示什麼、啟動哪些操作，由 card 清單描述——見 [裝置卡片：card 清單](../09-add-product/02-add-widget.md)。卡片動作的參數可以從選單項目取得範圍和預設值。

---

## 1. 韌體建置（`pio run`）

`menu.yaml` → `menu_gen.py` 將每個 `role:` 與合約中的 `canonical_roles` 核對 → 如有未知角色，建置失敗並列出有效角色 → 產生器把 C++ 檔案寫入 `src/menu/`。

## 2. 為門戶更新 TypeScript（`regen.sh`）

`mqtt_contract.yaml` → `gen_ts_types.py` 產生 `mqtt-api.types.ts` 和角色標籤 `roles.{lang}.json` → 檔案被複製到門戶。

合約變更時執行，並提交結果。

## 3. 執行時：裝置 ↔ 門戶

韌體把選單發布到 `config` 主題（由產品程式碼在收到 `get_config` 指令時完成；iDryer 產品在上線時也會發布）→ 門戶後端儲存 → 門戶透過 `GET /devices/:id/menu-config` 取得 → 每一項依類型 `t`（`val`、`tog`、`act`、`sub`）繪製，標籤依序取 `canonical_roles[r].labels[lang]`、英文、裝置傳來的名稱 `n`。

參數（`min`、`max`、`val`）來自選單項目本身——目前值由韌體掌握。

門戶透過 `commands/set { "id": <id>, "val": <value> }` 修改數值。

---

## 如何新增設定（NVS 參數）

```yaml
- id: my_param
  type: value
  role: my.param        # 選用：來自合約的標籤
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS 鍵（≤ 15 個字元）
  persist: true
  scope: global
  default: 50
```

`bind` = NVS 鍵。`persist: true` = 重新啟動後值仍保留。

`role:` 不是自由欄位：值必須來自合約中的 `canonical_roles`，否則建置失敗。清單見 `contracts/mqtt_contract.yaml` → `canonical_roles` 或 `menu.template.yaml`。新角色先加入合約，再執行 `regen.sh`。

---

## 如何在裝置卡片上新增操作

操作（啟動、停止、加熱、照明）宣告為卡片動作，而不是選單項目。其範圍可以取自選單項目：

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

完整說明 —— [裝置卡片：card 清單](../09-add-product/02-add-widget.md)。

---

## 不要做的事

- 不要在 `menu.yaml` 中新增 `widget:`。`canonical_roles` 的 `widget` 欄位僅供參考：門戶和應用程式都不讀取它。
- 不要手動編輯 `mqtt-api.types.ts` —— 它由 `regen.sh` 產生。
- 不要為新動作改動 `Config.hasXxx` 旗標 —— 它們只用於遙測（感測器、狀態）。
