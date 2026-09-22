# 绑定到账户

绑定是一次性的过程：设备获得一次性绑定令牌，在门户上用它换取永久密钥，并把密钥保存到 NVS。此后每次重启都会自行连接门户。没有密钥时，设备处于设置模式并等待令牌。

## 在 iDryer 应用中

1. 设备已在网络中（见 [Wi-Fi](01-wifi.md)），手机在同一网络。
2. **连接新设备** → **绑定** 步骤（如果设备已经在网络中，点击窗口顶部该步骤的标签）→ **绑定**。
3. 应用在局域网中找到设备，从门户获取令牌交给设备，并等待门户确认设备已上线。
4. 显示 **设备已绑定** 后，设备出现在门户和应用的列表中。

绑定前的日志：

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

固件的网页安装器通过 USB 用 `PAIR_TOKEN` 命令传递令牌（见下文）。

## 解绑

在应用或门户中解绑时，设备会收到 `revoke` 命令。每个固件都需要这个处理函数：

```cpp
// 在应用或门户中解绑：清除密钥，等待新的绑定。
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` 清除密钥并保留网络：设备重新等待绑定。没有处理函数时密钥会留在设备中，只有执行 `WIPE_IDENTITY` 之后才能重新绑定。

## USB 命令

连上网络后，核心在串口上接收命令行（115200，每行以换行结束）：

| 命令 | 回复 | 作用 |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | 状态：`state` 为 `bound`（已绑定）或 `setup`（等待令牌），`cloud` 为 `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | 像 `revoke` 一样清除密钥；网络保留 |
| `PAIR_TOKEN:<令牌>` | `PAIR_TOKEN:OK`、`PAIR_TOKEN:ERROR`、`PAIR_TOKEN:ERROR:ALREADY_BOUND` | 传入绑定令牌；已绑定的设备不接受 |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

连上网络之前端口被 Improv 占用，命令没有回复。在带 `IDRYER_DEV_REPL` 的构建中，端口归产品使用，这些命令不存在：见 [详细设置](99-detailed-setup.md)。

## 如果没有成功

- 应用找不到设备：手机和设备要在同一网络；访客网络经常阻止设备发现；
- 出现 `PAIR_TOKEN:ERROR:ALREADY_BOUND`，或设备属于另一个账户：在门户上解绑，或发送 `WIPE_IDENTITY` 后重新绑定。

## 下一步

[遥测](03-telemetry.md)。
