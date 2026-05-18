# 通過 devicePublisher 發佈

## 何時使用

`iDryer::Link` 已包含兩個內置傳輸：MQTT（雲）和本地 WebSocket (LAN)。大多數任務不需要額外的傳輸。

當產品組裝自己的有效負載並必須同時將其發送到兩個通道時，使用 `s_link.devicePublisher()` — 例如，在響應 `commands/get_config` 時發佈菜單配置。

## 現成代碼

```cpp
// main.cpp (片段)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// 通過單一調用將任意 JSON 有效負載發佈到 MQTT 和本地 WS。
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // 產品函數
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

單一的 `publishConfigRaw` 調用將有效負載傳遞到 MQTT 主題 `idryer/{serial}/config` 和所有活動的 LAN WS 客戶端。無需創建額外的客戶端或主題。

## 說明

`devicePublisher()` 是外觀的雙發佈幫助程序。除非您需要發佈到非標準主題，否則使用它代替直接調用 `mqttClient()` 或 `LocalAccess`。

遙測和狀態由外觀在計時器上自動發佈 — `devicePublisher()` 不需要用於這些。

## 何時需要第三種傳輸

添加第三個通道（BLE、Serial JSON、UART 代理）是外觀的架構擴展，而不是配方模式。絕大多數設備不需要這個。

如果您確實需要它 — 進入點在 `lib/idryer-core/src/cloud/`（雲狀態機、MQTT）和 `lib/idryer-core/src/`（本地訪問）。在繼續之前，確認內置的 MQTT 和本地 WS 對您的用例不夠。

## 存儲庫中的完整示例

`iDryer-Storage/src/main.cpp:171` 中的 `publishFullMenu()` — 通過 `s_link.devicePublisher()->publishConfigRaw(buf, len)` 發佈完整的 JSON 菜單。
