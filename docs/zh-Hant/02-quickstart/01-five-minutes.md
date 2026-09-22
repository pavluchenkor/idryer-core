---
title: "5 分鐘在 idryer-core 上執行裝置"
description: "idryer-core 上的第一台裝置：PlatformIO 專案、最小韌體，以及在 iDryer 應用程式中設定 Wi-Fi 並綁定帳戶。"
---

# 5 分鐘在 idryer-core 上執行裝置

完成本頁後，ESP32 會連上網路、綁定到你的帳戶，並出現在門戶 [portal.idryer.org](https://portal.idryer.org/) 和 iDryer 應用程式中。你需要：一塊 ESP32-C3 板（DevKit、Super Mini 或相容板）、一條 USB 線、VS Code 中的 PlatformIO、一支裝有 iDryer 應用程式的手機、一個 2.4 GHz 的 Wi-Fi 網路。

## 1. PlatformIO 專案

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← 副本、git 子模組或符號連結
└── src/
    └── main.cpp
```

`platformio.ini`：

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP 是 espMqttClient 相依套件中的 ESP8266 傳輸層，在 ESP32 上無法編譯。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; 沒有 USB-UART 的板子（ESP32-C3 SuperMini）：Serial 走 USB。
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

核心的函式庫（MQTT、ArduinoJson、WebSockets、Improv）會從核心的 `library.json` 自動引入。有 USB-UART 的板子不需要最後兩個旗標；`board` 依你的板子設定。

## 2. 程式碼

把範例 [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) 複製到 `src/main.cpp`：

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 你自己的裝置
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// 依狀態決定閃爍週期；0 表示不閃爍。
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // 解除綁定：核心清除密鑰，重新等待綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // 遙測欄位由核心自動發布，每 30 秒一次（閒置時每 60 秒一次）。
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

韌體中既沒有網路密碼，也沒有帳戶資料：裝置從應用程式取得網路和綁定。

## 3. 燒錄並開啟日誌

```bash
pio run -t upload
pio device monitor -b 115200
```

裝置沒有 Wi-Fi 時日誌保持安靜：核心把連接埠留給 Improv。LED 快速閃爍：裝置在等待網路和綁定。

## 4. 在應用程式中設定 Wi-Fi 並綁定

1. 把手機連接到裝置將要使用的 Wi-Fi 網路，並用門戶帳戶登入 iDryer 應用程式。
2. 在首頁點選 **连接新设备**——開啟 **Wi-Fi** 步驟。
3. 核對網路名稱，輸入密碼並點選 **连接设备**。應用程式最多傳送 90 秒；裝置加入網路後顯示 **设备已连接**。點選 **下一步**。
4. 在 **绑定** 步驟中點選 **绑定**。應用程式在網路中找到裝置，從門戶取得一次性綁定權杖並交給裝置。
5. 顯示 **设备已绑定** 後，裝置會出現在門戶和應用程式的裝置清單中。

連上網路後的日誌：

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

## 5. 檢查

- 裝置在門戶和應用程式中上線；
- 卡片顯示 ESP32 晶片溫度；
- LED 每秒閃一次。常亮或常滅表示與門戶沒有連線。

## 如果沒有成功

- 應用程式沒有等到裝置連線：檢查密碼以及網路是否為 2.4 GHz；密碼錯誤時裝置會重新等待設定。傳遞網路的其他方式見 [Wi-Fi](01-wifi.md)；
- **绑定** 步驟找不到裝置：手機和裝置必須在同一網路；訪客網路經常阻擋裝置探索。詳見 [綁定到帳戶](02-claim.md)；
- 編譯和旗標見 [詳細設定](99-detailed-setup.md)。

## 下一步

- [遙測](03-telemetry.md)：卡片上的感測器和自訂數值。
- [核心範例](https://github.com/pavluchenkor/idryer-core/tree/main/examples)：卡片操作、自訂感測器和控制項。
