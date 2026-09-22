# Wi-Fi

固件中没有网络密码。没有保存网络的设备会等待设置；收到后把网络保存到 NVS，之后自行连接。仅支持 2.4 GHz 网络。

## iDryer 应用（ESPTouch）

主要方式：应用通过无线方式发送网络，无需连线。

1. 手机连在设备将要使用的网络上。
2. **连接新设备** → **Wi-Fi** 步骤：核对网络名称，输入密码，点击 **连接设备**。
3. 应用最多发送 90 秒，并显示 **设备已连接**。

密码错误时设备会重新等待设置：重复这一步即可。

## 通过 USB 使用 Improv

设备没有网络时，核心在串口上监听 Improv 协议。

1. 用 USB 连接板子。
2. 在 Chrome 或 Edge 中打开 [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)（Safari 和 Firefox 不支持 Web Serial），点击 **Connect** 并选择板子的端口。
3. 输入网络名称和密码。

此时端口被 Improv 占用，所以日志要等设备连上网络后才出现。Improv 工作时请关闭串口监视器。

## 在代码中：用于开发台

```cpp
void setup() {
    // 仅用于开发台：NVS 中还没有网络时才保存。
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` 只在 NVS 中还没有网络时写入；`setWifiCredentials()` 总是覆盖。请在 `begin()` 之前调用。

!!! warning
    不要发布代码中带密码的固件：任何下载它的人都会拿到密码。

## 检查

日志中：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## 下一步

[绑定到账户](02-claim.md)。
