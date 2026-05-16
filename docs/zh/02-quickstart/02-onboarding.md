# 登錄：首次設備聲稱

登錄是一個一次性的過程，其中 ESP32 向 iDryer 雲端註冊並被聲稱到您的帳戶。完成後，設備出現在門戶中，狀態為線上，狀態為就緒，所有後續開機都是自動的。

## 您將需要什麼

- 一個用 REPL 構建閃存的 ESP32 設備：env `esp32c3-super-mini-dev`（參見 [5 分鐘快速開始](01-five-minutes.md)）或任何帶有 `IDRYER_DEV_REPL=1` 標誌的開發構建。
- USB 線纜。
- [portal.idryer.org](https://portal.idryer.org/) 帳戶（用於開發 — [staging.idryer.org](https://staging.idryer.org/)）。

## 路徑 1. 通過串行 REPL（推薦）

REPL 僅在帶有 `IDRYER_DEV_REPL=1` 標誌的構建中可用。打開串行監視器，輸入三個命令 — 設備連接到 WiFi，請求 PIN，並準備就緒聲稱。

### 1. 刷新開發構建

```bash
pio run -e esp32c3-super-mini-dev -t upload
```

或使用設置了 `-DIDRYER_DEV_REPL=1` 的任何 env。

### 2. 打開串行監視器

```bash
pio device monitor -b 115200
```

啟動後您將看到提示：

```
[boot] iDryer dev REPL ready — type 'help'
```

之後，雲堆棧消息立即開始出現在日誌中：

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```

### 3. 連接 WiFi

在串行監視器控制台中輸入：

```
wifi MyHomeWiFi MySecretPass
```

回應：

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

認證被寫入 NVS。主機板立即調用 `WiFi.begin()`。日誌將顯示：

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

### 4. 獲得 PIN 並在門戶中聲稱

設備會自動佈建並註冊一個 7 位數 PIN。PIN 有效期為 10 分鐘。

1. 打開 [portal.idryer.org](https://portal.idryer.org/)（或 staging）。
2. 轉到**添加設備**。
3. 從串行監視器輸入 PIN。

成功聲稱後，日誌顯示：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

如果 PIN 在您輸入前過期 — 運行 `claim` 命令以獲得新 PIN。

### 有用的 REPL 命令

| 命令 | 功能 | 何時使用 |
|---------|-------------|-------------|
| `help` | 顯示命令列表 | 提醒自己的語法 |
| `status` | 當前狀態：WiFi、IP、RSSI、線上、串行 | 連接診斷 |
| `wifi <ssid> <password>` | 將 WiFi 認證保存到 NVS 並重新連接 | 首次登錄或網絡變化 |
| `claim` | 手動啟動聲稱流程，獲得新 PIN | PIN 過期或需要重新聲稱 |
| `wipe` | 擦除 NVS（認證、聲稱、菜單）並重啟 | 出廠重置 |
| `restart` | ESP 的軟件重啟 | 無需物理斷開連接的快速重啟 |

## 路徑 2. 通過 Improv-WiFi（Web 串行）

Improv-WiFi 內置於所有構建中，不依賴 `IDRYER_DEV_REPL` 標誌。適合將設備移交給用戶或當終端不方便時。需要 Chrome 或 Edge — Safari 或 Firefox 不支持 Web Serial API。

### 1. 驗證主機板已刷新

任何 prod 構建都可以。Improv-WiFi 總是活躍的。

### 2. 打開網頁

轉到 [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/)，點擊**連接**，並在瀏覽器對話框中選擇設備的 USB 端口。

### 3. 輸入 SSID 和密碼

該頁面將要求網絡名稱和密碼，通過 Serial-Improv 將其傳輸到主機板。主機板將認證保存到 NVS 並連接到 WiFi。佈建和 PIN 檢索自動進行 — 與路徑 1 相同。

!!! note
    Improv-WiFi 無法運行 `claim`、`wipe` 或檢查 `status`。使用 REPL 進行手動聲稱流程和 NVS 管理。

### 何時使用每條路徑

| 情況 | 推薦 |
|-----------|---------------|
| 帶有終端打開的嵌入式開發人員 | REPL |
| 將設備交給用戶 | Improv-WiFi |
| 需要手動 `wipe` 或重複 `claim` | REPL |
| Safari 或 Firefox 瀏覽器 | REPL |
| 未安裝 PlatformIO | Improv-WiFi |

## 如果出現問題

**PIN 未出現在日誌中。** 檢查設備是否連接到 WiFi：輸入 `status` 並驗證回應中的 `ip=` 字段不為空。沒有 WiFi，佈建不會啟動。

**PIN 過期。** 輸入 `claim` 命令 — 設備請求新的註冊並打印新 PIN。

**設備已被聲稱到另一個帳戶。** 輸入 `wipe` — NVS 將被擦除，主機板將重啟並從頭開始登錄。

**PIN 不被門戶接受。** 驗證您複製了所有 7 位數字且沒有空格，並且自 PIN 出現以來已經過了不到 10 分鐘。

**Improv-WiFi 在瀏覽器中看不到設備。** 確保您使用的是 Chrome 或 Edge，並且 ESP32 USB 驅動程序已安裝。

## 下一步

- 完整 Link API：[../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- 添加傳感器或外設：[../04-patterns/](../04-patterns/)
