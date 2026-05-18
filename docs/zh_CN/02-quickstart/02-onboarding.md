# 登录：首次设备声称

登录是一个一次性的过程，其中 ESP32 向 iDryer 云端注册并被声称到您的帐户。完成后，设备出现在门户中，状态为在线，状态为就绪，所有后续开机都是自动的。

## 您将需要什么

- 一个用 REPL 构建刷新的 ESP32 设备：env `esp32c3-super-mini-dev`（参见 [5 分钟快速开始](01-five-minutes.md)）或任何带有 `IDRYER_DEV_REPL=1` 标志的开发构建。
- USB 电缆。
- [portal.idryer.org](https://portal.idryer.org/) 帐户（用于开发 — [staging.idryer.org](https://staging.idryer.org/)）。

## 路径 1. 通过串行 REPL（推荐）

REPL 仅在带有 `IDRYER_DEV_REPL=1` 标志的构建中可用。打开串行监视器，输入三个命令 — 设备连接到 WiFi，请求 PIN，并准备声称。

### 1. 刷新开发构建

```bash
pio run -e esp32c3-super-mini-dev -t upload
```

或使用设置了 `-DIDRYER_DEV_REPL=1` 的任何 env。

### 2. 打开串行监视器

```bash
pio device monitor -b 115200
```

启动后您将看到提示：

```
[boot] iDryer dev REPL ready — type 'help'
```

之后，云堆栈消息立即开始出现在日志中：

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```

### 3. 连接 WiFi

在串行监视器控制台中输入：

```
wifi MyHomeWiFi MySecretPass
```

回应：

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

凭证被写入 NVS。主板立即调用 `WiFi.begin()`。日志将显示：

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

### 4. 获得 PIN 并在门户中声称

设备会自动配置并注册一个 7 位数 PIN。PIN 有效期为 10 分钟。

1. 打开 [portal.idryer.org](https://portal.idryer.org/)（或 staging）。
2. 转到**添加设备**。
3. 从串行监视器输入 PIN。

成功声称后，日志显示：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

如果 PIN 在您输入前过期 — 运行 `claim` 命令以获得新 PIN。

### 有用的 REPL 命令

| 命令 | 功能 | 何时使用 |
|---------|-------------|-------------|
| `help` | 显示命令列表 | 提醒自己的语法 |
| `status` | 当前状态：WiFi、IP、RSSI、在线、串行 | 连接诊断 |
| `wifi <ssid> <password>` | 将 WiFi 凭证保存到 NVS 并重新连接 | 首次登录或网络变化 |
| `claim` | 手动启动声称流程，获得新 PIN | PIN 过期或需要重新声称 |
| `wipe` | 擦除 NVS（凭证、声称、菜单）并重启 | 出厂重置 |
| `restart` | ESP 的软件重启 | 无需物理断开连接的快速重启 |

## 路径 2. 通过 Improv-WiFi（Web 串行）

Improv-WiFi 内置于所有构建中，不依赖 `IDRYER_DEV_REPL` 标志。适合将设备移交给用户或当终端不方便时。需要 Chrome 或 Edge — Safari 或 Firefox 不支持 Web Serial API。

### 1. 验证主板已刷新

任何 prod 构建都可以。Improv-WiFi 总是活跃的。

### 2. 打开网页

转到 [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/)，点击**连接**，并在浏览器对话框中选择设备的 USB 端口。

### 3. 输入 SSID 和密码

该页面将要求网络名称和密码，通过 Serial-Improv 将其传输到主板。主板将凭证保存到 NVS 并连接到 WiFi。配置和 PIN 检索自动进行 — 与路径 1 相同。

!!! note
    Improv-WiFi 无法运行 `claim`、`wipe` 或检查 `status`。使用 REPL 进行手动声称流程和 NVS 管理。

### 何时使用每条路径

| 情况 | 推荐 |
|-----------|---------------|
| 带有终端打开的嵌入式开发人员 | REPL |
| 将设备交给用户 | Improv-WiFi |
| 需要手动 `wipe` 或重复 `claim` | REPL |
| Safari 或 Firefox 浏览器 | REPL |
| 未安装 PlatformIO | Improv-WiFi |

## 如果出现问题

**PIN 未出现在日志中。** 检查设备是否连接到 WiFi：输入 `status` 并验证回应中的 `ip=` 字段不为空。没有 WiFi，配置不会启动。

**PIN 过期。** 输入 `claim` 命令 — 设备请求新的注册并打印新 PIN。

**设备已被声称到另一个帐户。** 输入 `wipe` — NVS 将被擦除，主板将重启并从头开始登录。

**PIN 不被门户接受。** 验证您复制了所有 7 位数字且没有空格，并且自 PIN 出现以来已经过了不到 10 分钟。

**Improv-WiFi 在浏览器中看不到设备。** 确保您使用的是 Chrome 或 Edge，并且 ESP32 USB 驱动程序已安装。

## 下一步

- 完整 Link API：[../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- 添加传感器或外设：[../04-patterns/](../04-patterns/)
