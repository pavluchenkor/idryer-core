# 故障排除

使用 `idryer-core` 时的常见症状、其原因和解决方案。

阅读前，确保启用 HAL 日志(`idryer::hal::initArduinoHal(&Serial)`)并且在 `platformio.ini` 中设置 `-DCORE_DEBUG_LEVEL=3` 或更高。

## WiFi

### 状态机卡在 `WifiConnecting`

症状：日志重复 `state: WifiConnecting`，转换到 `Provisioning` 从不发生。

可能的原因：

- SSID/密码不正确。检查 `secrets.h` 中的 `WIFI_SSID` / `WIFI_PASSWORD`。在 Improv 配置后，凭证来自 NVS，而不是 `secrets.h`。
- 5 GHz 网络。ESP32 仅支持 2.4 GHz。
- 隐藏网络或路由器上的 MAC 过滤。
- `WiFi.begin()` 在 `idryer::hal::initArduinoHal(...)` 前调用 — 没有日志输出，但这不是挂起的原因，只是盲目。

要检查什么：

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFi 连接但在 30-60 秒后断开

通常：弱信号(`RSSI < -80 dBm`)、ESP32-C3 由没有专用 5V/1A 供应的 USB 集线器供电、与 FreeRTOS 任务的冲突。

在产品循环中记录 RSSI：

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## 配置和声称

### 状态机卡在 `Provisioning`

症状：`state: Provisioning` 不转换到 `Registering` 或 `AwaitingClaim`。

原因：

- `build_flags` 中的 `IDRYER_API_BASE` 不正确。必须是 `https://portal.idryer.org/api`(生产)或 `https://staging.idryer.org/api`(暂存)。
- 缺少 TLS 证书(Let's Encrypt ISRG Root X1)。嵌入在 `root_ca.h` 中，但在没有 `MQTT_USE_TLS` 的情况下构建时，HTTP 客户端也使用 TLS — 根 CA 也需要用于 HTTP API。
- 设备时间未同步(TLS 握手需要有效日期)。检查 `configTime(...)` 在 `setStateChangeCallback` 中在 `WifiConnecting` 之后调用(如在 Storage Link 中)。

### 状态机卡在 `AwaitingClaim`

用户未在门户中输入 PIN 时，这是正常状态。PIN 通过 `setClaimPinCallback` 打印到日志。

对于自动声称(没有 UI 的独立设备)：

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

在 `requestClaim()` 后，后端发出用户必须在门户中输入的 PIN。

### `seedSerialFromMac()` 生成了一个序列号，但在门户中输入的是不同的

NVS 中存储的序列号优先于 MAC 生成。`seedSerialFromMac()` 仅在尚未存在序列号时才写入 NVS。要更改序列号，清除 NVS：

```cpp
s_credentials.clear();
```

## MQTT

### 状态机进入 `MqttConnecting` 但未到达 `Online`

原因：

- 代理不可达。生产：`mqtt.idryer.org:8883`，暂存：`staging.idryer.org:1884`。
- `MQTT_USE_TLS=1` 但没有正确的根 CA — 握手无声地失败。
- 未应用 `setBufferSize(16384)` — `PubSubClient` 缓冲区默认为 256 字节。`MqttClient` 已设置 16384，但如果你直接使用 `PubSubClient` — 自己设置缓冲区。
- 持久会话在代理上"卡住"并有不同的客户端 ID。清除 NVS 并重新刷。

### 来自后端的命令未到达

检查订阅 — `MqttClient` 订阅 `idryer/{serial}/commands/#` 的 QoS 1。如果订阅失败，日志将显示：

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

验证 `setCommandHandler()` 在 `runtime.begin()` **之前**调用 — 否则第一批命令可能会被遗漏。

### `PubSubClient` 在恰好 60 秒的间隔断开

这是一个 keep-alive 超时。你的 MQTT 循环可能调用得不够频繁 — `s_runtime.loop()` 必须无长块地旋转。检查 `loop()` 没有 `delay(>500ms)` 和没有阻塞网络调用。

## 命令和处理器

### `commands/invoke` 到达但 `ActionDispatcher` 未被调用

如果你注册了 `setCommandHandler()`，**对 `ActionDispatcher` 的内置回退被禁用**。`IdryerRuntime` 传递所有内容(除了 `ping`)到你的 `CommandHandler`。你必须在其中显式调用 `s_dispatcher.handleInvoke(data)` 以处理 `invoke` 命令。

模板：

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... product commands ...
}
```

### `commands/set` 收到但配置未应用

`ActionDispatcher::handleSet` 提取 `id` 和 `val` 并传递给注册的 `SetCallback`。检查：

- `dispatcher.setSetCallback(onSetCommand, nullptr)` 在 `setup()` 中调用。
- `onSetCommand` 实际调用 `s_profile.applyConfig(id, val)`。
- `applyConfig` 为已知的 `id` 值返回 `true`。对于未知的返回 `false` 并忽略更改。

## 遥测

### 遥测未发布

`idryer-core` 不自动发布遥测。产品代码始终这样做。

检查：

- `pub.publishTelemetry(doc)`(或 `s_mqtt.publishTelemetry(doc)`，如果未使用 LocalAccess)在 `loop()` 中实际调用。
- 速率条件未阻止所有调用。常见错误：
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  在第一次通过 `lastTm == 0` 和 `millis()` 仍然很小 — 分支永不执行。使用 `>=` 并在第一次通过时初始化 `lastTm`。
- `s_runtime.isOnline() == true`。MQTT 在 Online 之前断开 — 发布不会通过。
- `JsonDocument` 大小足以容纳有效负载。检查 `doc.overflowed()` 在 `serializeJson` 之后。

### `publishTelemetry` 返回 `false`

原因：

- 未连接到代理(`MqttClient::isConnected() == false`)。
- 缓冲区超出 — 有效负载大于 `MQTT_BUFFER_SIZE`(16384 字节)。对于大数据使用 `publishConfigRaw`(带块)或减少有效负载。

### `DevicePublisher::publishTelemetry` 未到达 WS 客户端

`DevicePublisher` 如果 WS 客户端未连接则不返回错误 — 它只是跳过 WS 部分。检查 `s_local.isClientConnected()`。如果 `false` — 客户端未认证或未连接。

## NTP 和系统时间

### 设备时间未同步

NTP 同步在 `setStateChangeCallback` 中第一次退出 `WifiConnecting` 后启动：

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

如果此回调未注册 — 时间不自动同步。TLS 握手到代理需要有效时间；否则证书被认为已过期或来自未来。

替代通道：`IdryerRuntime` 处理 `commands/ping` 并通过 `settimeofday()` 应用 `data["timestamp"]`。如果后端每分钟发送一次 ping — 时间更新而无需 NTP。

### 长正常运行时间后 TLS 握手失败

如果 NTP 服务器不可达并且设备长期运行而不重启，时间可能漂移(特别是在没有 TCXO 的 ESP32-C3 上)。症状：几天正常运行时间后突然 `connection failed`。

解决方案：确保 `pool.ntp.org` 可从你的网络访问，或更频繁地从后端接收 `commands/ping`。

### `getIsoTimestamp` 返回年份 1970

系统时间尚未同步。时间在首次成功 `configTime` 或 `commands/ping` 后出现。直到那时，`info`/`telemetry` 将以占位符发布。

## ArduinoJson

### 编译错误：`StaticJsonDocument` 不是 `ArduinoJson` 的成员

你正在使用 ArduinoJson v7。`StaticJsonDocument` 类型仅存在于 v6。解决方案：

- 在 `platformio.ini` 中固定 v6：
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- 或将你的代码迁移到 v7 API(`JsonDocument` 而不是 `StaticJsonDocument<N>`)。`idryer-core` 为 v6 编写。

### 编译错误：歧义重载或类型不匹配

两个 ArduinoJson 版本可能通过传递依赖最终进入一个项目。检查：

```bash
pio pkg list -e my-device | grep -i arduinojson
```

必须有**一个**版本。如果有两个 — 通过 `lib_deps` 显式固定它，如果需要通过 `lib_ldf_mode = chain+` 或 `lib_ignore`。

### `doc.overflowed()` 在 serializeJson 之后为真

`StaticJsonDocument<N>` 大小对有效负载太小。增加 `N` 或对不频繁调用的路径使用 `DynamicJsonDocument`。

## 本地 WS (LocalAccess)

### 应用不发现 LAN 上的设备

mDNS 应在序列号可用后**立即启动**，通过 `s_local.initMdns(serial)`。检查：

- 路由器不阻止多播。
- 应用正在端口 81 上查找 `_idryer._tcp`。
- 设备序列号与在门户中注册的匹配。

### WS 客户端已连接但接收 `auth_required`

来自客户端的第一条消息必须是 `{"type":"auth","token":"<device_token>"}`。如果令牌无效，`LocalAccess` 调用 `setTokenRefreshCallback()`。产品必须在该回调中重新读取令牌从 `ICredentialStore` 并调用 `s_local.updateToken(...)`。

## 内存和稳定性

### 可用堆随时间减少

`PubSubClient::loop()` 和 `WebSocketsServer::loop()` 不应泄漏，但检查你的产品代码：

- 在堆栈上创建 `JsonDocument`(`StaticJsonDocument<N>`)，而不是在堆上(`DynamicJsonDocument`)用于频繁调用的路径。
- 产品代码中的 `String` 在 ESP32-C3 上快速碎片化堆 — 使用 `char[]` 和 `snprintf`。

### `Stack overflow` 或 `Guru Meditation`

`s_runtime.loop()` 不生成 FreeRTOS 任务 — 一切都在 Arduino 循环中运行。如果有堆栈崩溃，寻找：

- Arduino 循环堆栈上的大本地 `JsonDocument`/`char[8192]`(默认 8 KB)。
- 产品代码中的深递归。

增加 Arduino 循环堆栈：

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi(通过 Serial 的配置)

### Improv 不接受凭证

Improv 必须拥有 `Serial` 直到收到凭证：

```cpp
idryer::hal::initArduinoHal(nullptr);   // logs to /dev/null while Improv holds Serial
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // restore log output
}
```

如果 `HAL_LOG_*` 与 Improv 协议并行写入 `Serial`，Improv 在校验和失败。

### Improv 客户端不看到设备

检查 `setDeviceInfo` 中的 `ChipFamily`。必须与实际芯片匹配：`CF_ESP32_C3`、`CF_ESP32_S3`、`CF_ESP32_S2`、`CF_ESP32`。不匹配 — Improv 客户端不会在列表中显示设备。

还确保 Serial 波特率为 115200。Improv 协议期望这个。

## 集成诊断

### 完整诊断输出(1 Hz)

菜单 → `DIAGNOSTICS → DIAG LOG`(`menu.diag_en`)。默认禁用。

通过设备 UI、门户(`commands/set` 与 `bind=diag_en`)或 REPL(`set diag_en 1`)启用。

启用时，每秒打印一个块到 Serial：

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

用于远程诊断：用户启用 `DIAG LOG`、复制输出 → 连接器状态、lastError 和什么实际到达 RMT 是可见的。

### ANOMALY 通道(基于事件)

独立于 `diag_en`，连接器和助手在意外条件上写入带有前缀 `[!] ANOMALY` 的单独行：

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

`[!]` 前缀在常规日志流中视觉上突出异常。当某些东西"不工作"时，这是首先要在 Serial 中寻找的。

### 连接丧失时自动关闭(故障安全)

如果活动集成丧失连接(TCP/WS 断开)，连接器立即重置目标温度：

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`、`available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF。
- **Bambu** — 转换 `Connected → !Connected` → `chamberTarget=0`、`trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF。
- **HA** — 故障安全尚未实现。

没有这个逻辑，加热将以最后已知目标继续，直到连接恢复。

### Bambu：gcode_state 过滤器

`auto_heat` 仅在 `gcode_state == "RUNNING"` 或 `"PREPARE"` 时加热。

所有其他状态(`IDLE`、`FINISH`、`FAILED`、`PAUSE`、`INIT`、`OFFLINE`、`SLICING`、`UNKNOWN`、空) → OFF。

诊断时，注意 `[bambu]` 诊断行中的 `gcode_state` — 如果它显示 `IDLE`/`FINISH`，无论是否存在活动托盘，都不会有加热。

### 用于在没有打印机的情况下调试的测试台

用于在没有真实打印机的情况下测试集成，产品存储库(例如，iHeater-link)可能包含存根实用程序

如 `fake_moonraker` / `fake_bambu`，每 30 秒发送一个值范围。
