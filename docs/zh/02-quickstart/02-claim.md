# 步驟 02 — 聲稱：綁定到門戶

完成此步驟後，您的設備將在您的 [portal.idryer.org](https://portal.idryer.org/) 帳戶中顯示為線上狀態。所有後續重啟都是自動的 — 不需要重新聲稱。

## 什麼是聲稱

聲稱是一個一次性的過程，其中 ESP32 向 idryer.org 雲端註冊並綁定到您的帳戶。設備生成一個有效期為 10 分鐘的 7 位數 PIN。您在門戶中輸入 PIN — 綁定完成。

聲稱後，`deviceId` 被保存在 NVS 中 — 設備在雲中的唯一標識符。在後續重啟時，ESP32 直接連接到 MQTT，無需重複聲稱流程。

## 您需要什麼

- 從 [步驟 01](01-wifi.md) 刷新並連接到 WiFi 的 ESP32
- [portal.idryer.org](https://portal.idryer.org/) 上的帳戶
- USB 線纜和打開的串行監視器

## 步驟

**1. 驗證草圖包含自動聲稱。** 以下行必須在 `setup()` 中（它已經在 `03_with_improv` 示例中存在）：

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

當設備連接到互聯網並檢測到尚未被聲稱時，此回調會自動觸發。

**2. 打開串行監視器**並重啟主機板：

```bash
pio device monitor -b 115200
```

**3. 在日誌中等待 PIN。** 在 WiFi → 佈建 → 等待聲稱之後：

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

設備正在等待。PIN 有效期為 10 分鐘。

**4. 轉到 [portal.idryer.org](https://portal.idryer.org/)**並導航到**添加設備**。

**5. 從串行監視器輸入 PIN**（7 位數字，無空格）。

**6. 在門戶中確認綁定**。串行監視器將顯示：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

## 驗證

打開門戶上的設備列表 — 設備應顯示為**線上**狀態。內置 LED 將每 500 毫秒閃爍一次（如果您正在使用 `01_blink_status` 示例）。

!!! note
    如果 PIN 過期（已超過 10 分鐘） — 重啟主機板。自動聲稱將生成新 PIN。

!!! warning
    如果設備已被另一個帳戶聲稱，在啟用 `IDRYER_DEV_REPL=1` 的串行監視器中輸入 `wipe` 命令。NVS 將被擦除，主機板將重啟，聲稱將從新開始。

## 下一步

- [03-telemetry.md](03-telemetry.md) — 連接傳感器並將讀數發布到門戶。
- [02-onboarding.md](02-onboarding.md) — REPL 和 Improv 路徑的詳細登錄文檔。
