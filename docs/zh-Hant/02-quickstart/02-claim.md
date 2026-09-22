# 綁定到帳戶

綁定是一次性的流程：裝置取得一次性綁定權杖，在門戶上用它換取永久密鑰，並把密鑰儲存到 NVS。此後每次重新啟動都會自行連線到門戶。沒有密鑰時，裝置處於設定模式並等待權杖。

## 在 iDryer 應用程式中

1. 裝置已在網路中（見 [Wi-Fi](01-wifi.md)），手機在同一網路。
2. **连接新设备** → **绑定** 步驟（如果裝置已經在網路中，點選視窗頂部該步驟的標籤）→ **绑定**。
3. 應用程式在區域網路中找到裝置，從門戶取得權杖交給裝置，並等待門戶確認裝置已上線。
4. 顯示 **设备已绑定** 後，裝置出現在門戶和應用程式的清單中。

綁定前的日誌：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

綁定後：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

韌體的網頁安裝程式透過 USB 用 `PAIR_TOKEN` 命令傳遞權杖（見下文）。

## 解除綁定

在應用程式或門戶中解除綁定時，裝置會收到 `revoke` 命令。每個韌體都需要這個處理函式：

```cpp
// 在應用程式或門戶中解除綁定：清除密鑰，等待新的綁定。
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` 清除密鑰並保留網路：裝置重新等待綁定。沒有處理函式時密鑰會留在裝置中，只有執行 `WIPE_IDENTITY` 之後才能重新綁定。

## USB 命令

連上網路後，核心在序列埠上接收命令列（115200，每行以換行結束）：

| 命令 | 回覆 | 作用 |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | 狀態：`state` 為 `bound`（已綁定）或 `setup`（等待權杖），`cloud` 為 `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | 像 `revoke` 一樣清除密鑰；網路保留 |
| `PAIR_TOKEN:<權杖>` | `PAIR_TOKEN:OK`、`PAIR_TOKEN:ERROR`、`PAIR_TOKEN:ERROR:ALREADY_BOUND` | 傳入綁定權杖；已綁定的裝置不接受 |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

連上網路之前連接埠被 Improv 占用，命令沒有回覆。在帶有 `IDRYER_DEV_REPL` 的建置中，連接埠歸產品使用，這些命令不存在：見 [詳細設定](99-detailed-setup.md)。

## 如果沒有成功

- 應用程式找不到裝置：手機和裝置要在同一網路；訪客網路經常阻擋裝置探索；
- 出現 `PAIR_TOKEN:ERROR:ALREADY_BOUND`，或裝置屬於另一個帳戶：在門戶上解除綁定，或傳送 `WIPE_IDENTITY` 後重新綁定。

## 下一步

[遙測](03-telemetry.md)。
