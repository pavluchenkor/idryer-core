# 故障排除

使用 `idryer-core` 时的常见症状、原因和解决方法。

阅读前请确认 HAL 日志已启用（`idryer::hal::initArduinoHal(&Serial)`），并且 `platformio.ini` 中设置了 `-DCORE_DEBUG_LEVEL=3` 或更高。

## WiFi

### 状态机卡在 `WifiConnecting`

症状：日志重复 `state: WifiConnecting`，始终不进入 `Provisioning`。

可能原因：

- SSID/密码错误。检查 `secrets.h` 中的 `WIFI_SSID` / `WIFI_PASSWORD`。Improv provisioning 之后，凭据来自 NVS，而不是 `secrets.h`。
- 使用 5 GHz 网络。ESP32 只支持 2.4 GHz。
- 路由器上隐藏网络或启用了 MAC 过滤。
- 在 `idryer::hal::initArduinoHal(...)` 之前调用了 `WiFi.begin()`；这会导致没有日志输出，但不是卡住的原因，只是看不见问题。

检查项：

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFi 已连接，但 30–60 秒后掉线

典型原因：信号弱（`RSSI < -80 dBm`）、ESP32-C3 通过没有独立 5V/1A 电源的 USB hub 供电、与 FreeRTOS task 冲突。

在产品 loop 中记录 RSSI：

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## Provisioning 和 claim

### 状态机卡在 `Provisioning`

症状：`state: Provisioning`，但不进入 `Registering` 或 `AwaitingClaim`。

原因：

- `build_flags` 中的 `IDRYER_API_BASE` 错误。生产环境必须是 `https://portal.idryer.org/api`，staging 必须是 `https://staging.idryer.org/api`。
- 缺少 TLS 证书（Let's Encrypt ISRG Root X1）。它嵌入在 `root_ca.h` 中；即使未用 `MQTT_USE_TLS` 构建，HTTP client 也使用 TLS，因此 HTTP API 同样需要根 CA。
- 设备时间未同步（TLS 握手需要有效日期）。检查在 `WifiConnecting` 之后的 `setStateChangeCallback` 中是否调用 `configTime(...)`（如 Storage Link 中那样）。

### 状态机卡在 `AwaitingClaim`

当用户尚未在门户中输入 PIN 时，这是正常状态。PIN 会通过 `setClaimPinCallback` 打印到日志。

自动 claim（无 UI 的独立设备）：

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

调用 `requestClaim()` 后，后端会发出 PIN，用户必须在门户中输入。

### `seedSerialFromMac()` 生成了序列号，但门户中输入了另一个

NVS 中保存的序列号优先于 MAC 生成。`seedSerialFromMac()` 只会在尚无序列号时写入 NVS。要更改序列号，请清空 NVS：

```cpp
s_credentials.clear();
```

## MQTT

### 状态机进入 `MqttConnecting`，但没有到达 `Online`

原因：

- Broker 不可达。生产环境：`mqtt.idryer.org:8883`，staging：`staging.idryer.org:1884`。
- `MQTT_USE_TLS=1` 但根 CA 不正确，握手会静默失败。
- 未应用 `setBufferSize(16384)`；`PubSubClient` 默认缓冲区是 256 字节。`MqttClient` 已设置 16384，但如果直接使用 `PubSubClient`，请自行设置缓冲区。
- Broker 上有使用不同 client ID 的持久会话卡住。清空 NVS 并重新刷写。

### 后端命令没有到达

检查订阅：`MqttClient` 以 QoS 1 订阅 `idryer/{serial}/commands/#`。如果订阅失败，日志会显示：

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

确认 `setCommandHandler()` 在 `runtime.begin()` **之前**调用，否则第一批命令可能会错过。

### `PubSubClient` 每隔正好 60 秒断开

这是 keep-alive 超时。MQTT loop 可能调用得不够频繁；`s_runtime.loop()` 必须在没有长时间阻塞的情况下持续运行。检查 `loop()` 中没有 `delay(>500ms)`，也没有阻塞式网络调用。

## 命令和 handler

### 收到 `commands/invoke`，但没有调用 `ActionDispatcher`

如果注册了 `setCommandHandler()`，**内置的 `ActionDispatcher` fallback 会被禁用**。`IdryerRuntime` 会把除 `ping` 以外的所有内容传给你的 `CommandHandler`。你必须在其中为 `invoke` 命令显式调用 `s_dispatcher.handleInvoke(data)`。

模板：

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... product commands ...
}
```

### 收到 `commands/set`，但配置没有应用

`ActionDispatcher::handleSet` 提取 `id` 和 `val`，并传给注册的 `SetCallback`。检查：

- `dispatcher.setSetCallback(onSetCommand, nullptr)` 是否在 `setup()` 中调用。
- `onSetCommand` 是否确实调用 `s_profile.applyConfig(id, val)`。
- `applyConfig` 对已知 `id` 返回 `true`。对未知值返回 `false`，修改会被忽略。

## 遥测

### 遥测没有发布

`idryer-core` 不会自动发布遥测。始终由产品代码发布。

检查：

- `loop()` 中是否真的调用了 `pub.publishTelemetry(doc)`（或未使用 LocalAccess 时调用 `s_mqtt.publishTelemetry(doc)`）。
- 频率条件是否阻塞了所有调用。常见错误：
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  第一次执行时 `lastTm == 0`，而 `millis()` 仍然很小，因此分支永远不会执行。使用 `>=`，并在第一次执行时初始化 `lastTm`。
- `s_runtime.isOnline() == true`。Online 前 MQTT 未连接，发布不会成功。
- `JsonDocument` 大小足以容纳 payload。`serializeJson` 后检查 `doc.overflowed()`。

### `publishTelemetry` 返回 `false`

原因：

- 未连接到 broker（`MqttClient::isConnected() == false`）。
- 缓冲区超限：payload 大于 `MQTT_BUFFER_SIZE`（16384 字节）。大数据请使用 `publishConfigRaw`（分块）或减少 payload。

### `DevicePublisher::publishTelemetry` 没有到达 WS 客户端

如果 WS 客户端未连接，`DevicePublisher` 不返回错误，只会跳过 WS 部分。检查 `s_local.isClientConnected()`。如果为 `false`，客户端未认证或未连接。

## NTP 和系统时间

### 设备时间未同步

NTP 同步在第一次离开 `WifiConnecting` 后的 `setStateChangeCallback` 中启动：

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

如果未注册此 callback，时间不会自动同步。Broker 的 TLS 握手需要有效时间，否则证书会被认为已过期或来自未来。

替代通道：`IdryerRuntime` 会处理 `commands/ping`，并通过 `settimeofday()` 应用 `data["timestamp"]`。如果后端每分钟发送一次 ping，时间就可以不依赖 NTP 更新。

### 长时间运行后 TLS 握手失败

如果 NTP 服务器不可达，且设备长时间不重启，时间可能漂移（尤其是没有 TCXO 的 ESP32-C3）。症状是运行数天后突然 `connection failed`。

解决：确保网络可访问 `pool.ntp.org`，或更频繁地从后端接收 `commands/ping`。

### `getIsoTimestamp` 返回 1970 年

系统时间尚未同步。第一次成功 `configTime` 或 `commands/ping` 后才会有真实时间。在此之前，`info`/`telemetry` 会带占位时间发布。

## ArduinoJson

### 编译错误：`StaticJsonDocument` 不是 `ArduinoJson` 的成员

你使用的是 ArduinoJson v7。`StaticJsonDocument` 类型只存在于 v6。解决方法：

- 在 `platformio.ini` 中固定 v6：
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- 或将代码迁移到 v7 API（用 `JsonDocument` 替代 `StaticJsonDocument<N>`）。`idryer-core` 是按 v6 编写的。

### 编译错误：重载歧义或类型不匹配

一个项目可能通过传递依赖同时带入两个版本的 ArduinoJson。检查：

```bash
pio pkg list -e my-device | grep -i arduinojson
```

必须只有**一个**版本。如果有两个，请通过 `lib_deps` 显式固定，必要时再使用 `lib_ldf_mode = chain+` 或 `lib_ignore`。

### `serializeJson` 后 `doc.overflowed()` 为 true

`StaticJsonDocument<N>` 太小，装不下 payload。增大 `N`，或在低频路径中使用 `DynamicJsonDocument`。

## 本地 WS (LocalAccess)

### App 在 LAN 上发现不了设备

mDNS 应在序列号可用后**立即**通过 `s_local.initMdns(serial)` 启动。检查：

- 路由器没有阻止 multicast。
- App 正在 81 端口查找 `_idryer._tcp`。
- 设备序列号与门户中注册的一致。

### WS 客户端已连接但收到 `auth_required`

客户端的第一条消息必须是 `{"type":"auth","token":"<device_token>"}`。如果 token 无效，`LocalAccess` 会调用 `setTokenRefreshCallback()`。产品必须在该 callback 中从 `ICredentialStore` 重新读取 token，并调用 `s_local.updateToken(...)`。

## 内存和稳定性

### Free heap 随时间下降

`PubSubClient::loop()` 和 `WebSocketsServer::loop()` 不应泄漏，但请检查产品代码：

- 高频路径中在栈上创建 `JsonDocument`（`StaticJsonDocument<N>`），不要在 heap 上创建（`DynamicJsonDocument`）。
- ESP32-C3 上产品代码里的 `String` 会很快造成 heap 碎片；使用 `char[]` 和 `snprintf`。

### `Stack overflow` 或 `Guru Meditation`

`s_runtime.loop()` 不会创建 FreeRTOS task；所有内容都在 Arduino loop 中运行。如果出现栈崩溃，请查找：

- Arduino loop 栈上的大局部 `JsonDocument` / `char[8192]`（默认 8 KB）。
- 产品代码中的深递归。

增大 Arduino loop 栈：

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi（通过 Serial provisioning）

### Improv 不接受凭据

在收到凭据之前，Improv 必须独占 `Serial`：

```cpp
idryer::hal::initArduinoHal(nullptr);   // logs to /dev/null while Improv holds Serial
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // restore log output
}
```

如果 `HAL_LOG_*` 与 Improv 协议同时写入 `Serial`，Improv 会因 checksum 失败。

### Improv 客户端看不到设备

检查 `setDeviceInfo` 中的 `ChipFamily`。必须匹配真实芯片：`CF_ESP32_C3`、`CF_ESP32_S3`、`CF_ESP32_S2`、`CF_ESP32`。不匹配时，Improv 客户端不会在列表中显示设备。

还要确认 Serial baudrate 是 115200。Improv 协议期望这个速率。

## 集成诊断

### 完整诊断输出（1 Hz）

菜单 → `DIAGNOSTICS → DIAG LOG`（`menu.diag_en`）。默认关闭。
可通过设备 UI、门户（`commands/set`，`bind=diag_en`）或 REPL（`set diag_en 1`）启用。

启用后，每秒会向 Serial 打印一个块：

```
=========== iHeater Link diagnostics ===========
[device]    serial=DEVICE_... online=1 uptime=42s
[wifi]      status=3 ssid=Apart_4 ip=192.168.0.140 rssi=-51
[rmt-out]   mode=DRYING target=70.0°C
[active]    bambu
[bambu]     state=CONNECTED  ip=192.168.0.171 serial=<set> lan=<set>
            gcode_state='RUNNING' tray='PLA' chamber_target=0.0 chamber_temp=0.0
[moonraker] state=DISABLED   ws=ws://192.168.0.171:7125
            vc.available=0 vc.target=0.0 vc.temp=0.0 vc.has_sensor=0
[ha]        state=DISABLED   host=<empty>:1883 user=<empty>
[menu]      bambu_en=1 moon_en=0 ha_en=0 diag_en=1  mat_pla=45 ...
================================================
```

这对远程诊断有用：用户启用 `DIAG LOG`，复制输出，就能看到 connector 状态、lastError，以及实际送往 RMT 的内容。

### ANOMALY 通道（基于事件）

不依赖 `diag_en`，connector 和辅助器会在异常条件下写出带 `[!] ANOMALY` 前缀的独立行：

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

`[!]` 前缀会在普通日志流中突出显示异常。这是 Serial 中排查“无法工作”时应首先查找的内容。

### 连接丢失时自动关闭（fail-safe）

如果活动集成丢失连接（TCP/WS 断开），connector 会立即重置目标温度：

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`，`available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF。
- **Bambu** — `Connected → !Connected` 转换 → `chamberTarget=0`，`trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF。
- **HA** — fail-safe 尚未实现。

没有这段逻辑时，加热会一直按最后已知目标继续，直到连接恢复。

### Bambu：gcode_state 过滤

`auto_heat` 只在 `gcode_state == "RUNNING"` 或 `"PREPARE"` 时加热。
所有其他状态（`IDLE`、`FINISH`、`FAILED`、`PAUSE`、`INIT`、`OFFLINE`、`SLICING`、`UNKNOWN`、空值）→ OFF。

诊断时注意 `[bambu]` 诊断行中的 `gcode_state`；如果显示 `IDLE`/`FINISH`，即使存在活动托盘也不会加热。

### 无打印机调试用测试台

为了在没有真实打印机时测试集成，产品仓库（例如 iHeater-link）可能包含 stub 工具，例如 `fake_moonraker` / `fake_bambu`，它们每 30 秒发送一组递增/变化的值。
