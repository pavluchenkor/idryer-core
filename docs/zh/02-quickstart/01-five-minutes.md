---
title: "5 分钟在 idryer-core 上运行设备"
description: "idryer-core 上的第一台设备：PlatformIO 项目、最小固件，以及在 iDryer 应用中配置 Wi-Fi 并绑定账户。"
---

# 5 分钟在 idryer-core 上运行设备

完成本页后，ESP32 会连上网络、绑定到你的账户，并出现在门户 [portal.idryer.org](https://portal.idryer.org/) 和 iDryer 应用中。你需要：一块 ESP32-C3 板（DevKit、Super Mini 或兼容板）、一根 USB 线、VS Code 中的 PlatformIO、一部装有 iDryer 应用的手机、一个 2.4 GHz 的 Wi-Fi 网络。

## 1. PlatformIO 项目

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← 副本、git 子模块或符号链接
└── src/
    └── main.cpp
```

`platformio.ini`：

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP 是 espMqttClient 依赖中的 ESP8266 传输层，在 ESP32 上无法编译。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; 没有 USB-UART 的板子（ESP32-C3 SuperMini）：Serial 走 USB。
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

核心的库（MQTT、ArduinoJson、WebSockets、Improv）会从核心的 `library.json` 自动引入。带 USB-UART 的板子不需要最后两个标志；`board` 按你的板子设置。

## 2. 代码

把示例 [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) 复制到 `src/main.cpp`：

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 你自己的设备
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// 按状态决定闪烁周期；0 表示不闪烁。
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // 解绑：核心清除密钥，重新等待绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // 遥测字段由核心自动发布，每 30 秒一次（空闲时每 60 秒一次）。
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

固件中既没有网络密码，也没有账户信息：设备从应用获得网络和绑定。

## 3. 烧录并打开日志

```bash
pio run -t upload
pio device monitor -b 115200
```

设备没有 Wi-Fi 时日志保持安静：核心把端口留给 Improv。LED 快速闪烁：设备在等待网络和绑定。

## 4. 在应用中配置 Wi-Fi 并绑定

1. 把手机连接到设备将要使用的 Wi-Fi 网络，并用门户账户登录 iDryer 应用。
2. 在首页点击 **连接新设备**——打开 **Wi-Fi** 步骤。
3. 核对网络名称，输入密码并点击 **连接设备**。应用最多发送 90 秒；设备加入网络后显示 **设备已连接**。点击 **下一步**。
4. 在 **绑定** 步骤中点击 **绑定**。应用在网络中找到设备，从门户获取一次性绑定令牌并交给设备。
5. 显示 **设备已绑定** 后，设备会出现在门户和应用的设备列表中。

连上网络后的日志：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

绑定后：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. 检查

- 设备在门户和应用中在线；
- 卡片显示 ESP32 芯片温度；
- LED 每秒闪一次。常亮或常灭表示与门户没有连接。

## 如果没有成功

- 应用没有等到设备连接：检查密码以及网络是否为 2.4 GHz；密码错误时设备会重新等待设置。传递网络的其他方式见 [Wi-Fi](01-wifi.md)；
- **绑定** 步骤找不到设备：手机和设备必须在同一网络；访客网络经常阻止设备发现。详见 [绑定到账户](02-claim.md)；
- 编译和标志见 [详细设置](99-detailed-setup.md)。

## 下一步

- [遥测](03-telemetry.md)：卡片上的传感器和自定义数值。
- [核心示例](https://github.com/pavluchenkor/idryer-core/tree/main/examples)：卡片操作、自定义传感器和控件。
