# 新增周邊設備

## 何時使用

如果裝置需要根據來自雲端或 LAN 的命令控制硬體（繼電器、加熱器、LED 條、馬達），請使用此配方。

## 現成可用的代碼

（相同的 C++ 代碼，不翻譯）

[見英文版本的代碼]

## 說明

`s_link.runtime()->setCommandHandler(handleCommand)` 是命令處理程式的單一連線點。在此呼叫後，所有傳入的 MQTT 命令（`invoke`、`set`、`drying`、`stop`、`ping`、`get_config` 等）直接到達 `handleCommand`。

`s_link.publishStatusNow()` — 在每次變更 `s_link.status.*` 後呼叫。這立即將新狀態發送到入口網站和 LAN 客戶端，而無需等待 `statusPeriodMs` 計時器。

永遠不要在 `handleCommand` 內呼叫 `delay()`。此呼叫是從 MQTT 回呼同步的；阻止它會中斷工作階段。將計時器放在產品物件的 `loop()` 中。

### 替代方案：`link.onRequest()`

對於標準命令（`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`），更簡單的 `onRequest()` 回呼就足夠了 — 無需解析原始 JSON：

[見英文版本的代碼]

`onRequest()` 不能與 `setCommandHandler` 同時運作 — 如果設定了完整處理程式，則不會呼叫 `onRequest` 回呼。詳情請參閱 [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)。

## 儲存庫中的完整示例

參考實現：`iHeater-link/src/main.cpp` 中的 `handleCommand` 處理 `drying` / `stop`。
