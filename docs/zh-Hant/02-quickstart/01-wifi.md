# Wi-Fi

韌體中沒有網路密碼。沒有儲存網路的裝置會等待設定；收到後把網路儲存到 NVS，之後自行連線。僅支援 2.4 GHz 網路。

## iDryer 應用程式（ESPTouch）

主要方式：應用程式以無線方式傳送網路，不需接線。

1. 手機連在裝置將要使用的網路上。
2. **连接新设备** → **Wi-Fi** 步驟：核對網路名稱，輸入密碼，點選 **连接设备**。
3. 應用程式最多傳送 90 秒，並顯示 **设备已连接**。

密碼錯誤時裝置會重新等待設定：重做這一步即可。

## 透過 USB 使用 Improv

裝置沒有網路時，核心在序列埠上監聽 Improv 協定。

1. 用 USB 連接板子。
2. 在 Chrome 或 Edge 中開啟 [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)（Safari 和 Firefox 不支援 Web Serial），點選 **Connect** 並選擇板子的連接埠。
3. 輸入網路名稱和密碼。

此時連接埠被 Improv 占用，所以日誌要等裝置連上網路後才出現。Improv 運作時請關閉序列監視器。

## 在程式碼中：用於開發台

```cpp
void setup() {
    // 僅用於開發台：NVS 中還沒有網路時才儲存。
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` 只在 NVS 中還沒有網路時寫入；`setWifiCredentials()` 一律覆寫。請在 `begin()` 之前呼叫。

!!! warning
    不要發布程式碼中帶有密碼的韌體：任何下載它的人都會拿到密碼。

## 檢查

日誌中：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## 下一步

[綁定到帳戶](02-claim.md)。
