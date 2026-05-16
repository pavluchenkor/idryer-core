# MQTT 合約

文件 `contracts/mqtt_contract.yaml` 是 `idryer-core` MQTT 接口的真實來源。

## 範圍

此合約僅描述 **`idryer-core` 中 `MqttClient` 實現的內容**:

- 庫可以發佈到的主題
- 庫接受和路由的命令

完整的平台接口 (所有設備類型的所有後端命令，包括 `drying`, `storage`, `profile`, `rfid` 等) 在 `contracts/portal_backend_status.md` 中 — 這是 [平台參考]。

## 設備主題 (設備 → 後端)

| 後綴 | 保留 | 狀態 |
|--------|----------|--------|
| `info` | 是 | 已實現 |
| `telemetry` | 否 | 已實現 |
| `status` | 是 | 已實現 |
| `config` | 否 | 已實現 |
| `config/delta` | 否 | 已實現 |
| `events` | 否 | 已實現 |
| `integrations/status` | 是 | 已實現 |
| `offline` (LWT) | 否 | 已實現 |

## 命令 (後端 → 設備)

| 後綴 | 處理程序 | 狀態 |
|--------|---------|--------|
| `commands/ping` | `IdryerRuntime` (內置) | 已實現 |
| `commands/invoke` | 產品 `CommandHandler` (推薦); 後備 → `ActionDispatcher` | 已實現 |
| `commands/set` | 產品 `CommandHandler` (推薦); 後備 → `ActionDispatcher` | 已實現 |
| `commands/link_integration` | `LinkIntegrationsManager` via `CommandHandler` | 已實現 |
| `commands/bambu_apply` | `LinkIntegrationsManager` via `CommandHandler` | 已實現 |
| 其他所有 | 產品 `CommandHandler` | 產品定義 |

## 變更規則

`idryer-core` 中 MQTT 協議的任何變更必須同時接觸:

1. `contracts/mqtt_contract.yaml`
2. 庫代碼 (`mqtt_client.h/.cpp`)
3. 門戶 / 後端代碼

首先更新合約，然後更新代碼。

## 相容性

- 向有效負載添加新的可選字段是安全的。
- 重命名現有字段需要同時更新韌體、門戶和合約。
- `info` 和 `config` 有效負載由產品定義 — `idryer-core` 不驗證它們。
