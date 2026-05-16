# 步骤 02 — 声称：绑定到门户

完成此步骤后，您的设备将在您的 [portal.idryer.org](https://portal.idryer.org/) 帐户中显示为在线状态。所有后续重启都是自动的 — 不需要重新声称。

## 什么是声称

声称是一个一次性的过程，其中 ESP32 向 idryer.org 云端注册并绑定到您的帐户。设备生成一个有效期为 10 分钟的 7 位数 PIN。您在门户中输入 PIN — 绑定完成。

声称后，`deviceId` 被保存在 NVS 中 — 设备在云中的唯一标识符。在后续重启时，ESP32 直接连接到 MQTT，无需重复声称流程。

## 您需要什么

- 从 [步骤 01](01-wifi.md) 刷新并连接到 WiFi 的 ESP32
- [portal.idryer.org](https://portal.idryer.org/) 上的帐户
- USB 电缆和打开的串行监视器

## 步骤

**1. 验证草图包含自动声称。** 以下行必须在 `setup()` 中（它已经在 `03_with_improv` 示例中存在）：

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

当设备连接到互联网并检测到尚未被声称时，此回调会自动触发。

**2. 打开串行监视器**并重启主板：

```bash
pio device monitor -b 115200
```

**3. 在日志中等待 PIN。** 在 WiFi → 配置 → 等待声称之后：

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

设备正在等待。PIN 有效期为 10 分钟。

**4. 转到 [portal.idryer.org](https://portal.idryer.org/)**并导航到**添加设备**。

**5. 从串行监视器输入 PIN**（7 位数字，无空格）。

**6. 在门户中确认绑定**。串行监视器将显示：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

## 验证

打开门户上的设备列表 — 设备应显示为**在线**状态。内置 LED 将每 500 毫秒闪烁一次（如果您正在使用 `01_blink_status` 示例）。

!!! note
    如果 PIN 过期（已超过 10 分钟） — 重启主板。自动声称将生成新 PIN。

!!! warning
    如果设备已被另一个帐户声称，在启用 `IDRYER_DEV_REPL=1` 的串行监视器中输入 `wipe` 命令。NVS 将被擦除，主板将重启，声称将从新开始。

## 下一步

- [03-telemetry.md](03-telemetry.md) — 连接传感器并将读数发布到门户。
- [02-onboarding.md](02-onboarding.md) — REPL 和 Improv 路径的详细登录文档。
