# 什麼是 idryer-core

如果您正在為 iDryer 雲端構建 ESP32 設備，此庫處理 WiFi 佈建（Improv）、聲明協議、MQTT 會話（TLS、重新連接、時間同步）、定期遙測/狀態發佈和傳入命令路由。大約 500 行的樣板代碼簡化為 `link.begin(); link.loop();`。

## 最小示例

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## 庫的功能

- WiFi 連接和保活；初始設置的 Web Serial Improv 佈建。
- 聲明協議：後端中的設備註冊，通過 PIN 聲明帳戶。
- 與 iDryer 代理的 MQTT 會話：TLS、永久會話、自動重新連接、NTP 時間同步。
- 定期發佈遙測（`Telemetry`）和狀態（`Status`）。
- 將傳入命令（`commands/invoke`、`commands/set`、`commands/ping`）路由到產品處理器。
- 本地 WebSocket 服務器：LAN 客戶端看到與雲相同的流。
- NVS 持久化：WiFi 認證、設備令牌、重啟間隔的菜單配置。

## 庫不做什麼

- 不管理產品硬件：風扇、加熱器、LED 條、傳感器。
- 不包含乾燥、存儲或照明的業務邏輯。
- 不知道產品特定的菜單參數 — 只是傳輸它們。
- 沒有來自產品的數據不發佈遙測：在 `loop()` 中自己填充 `link.telemetry.*`。

## 下一步

- [5 分鐘快速開始](../02-quickstart/01-five-minutes.md)
- [完整 API：iDryer::Link](../03-public-api/01-link-api-reference.md)
