# 新しい製品を追加する方法

`idryer-core` の上に新しいデバイスを構築するための実践的なチェックリスト。

2つのシナリオ:

- **最小限** — MQTT + クラウドのみ。ほとんどのシンプルなデバイスに十分です。
- **拡張** — MQTT + LAN経由のローカルWS アクセス。クラウドなしでローカルアクセスが必要なデバイス用です。

---

## シナリオ1: 最小限のMQTT専用デバイス

最小セット: WiFi、MQTT、クラウド状態マシン、1つのプロファイル。

リファレンス: [`examples/minimal_mqtt_only/`](../../../examples/minimal_mqtt_only/)

### 1. IProfileを実装

```cpp
// src/mydevice/my_profile.h
#include <profiles/IProfile.h>

class MyProfile : public idryer::IProfile {
public:
    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;
};
```

### 2. コンポジションルートをアセンブル

```cpp
#include <idryer_core.h>

static idryer::ArduinoWifiStore       s_wifiStore;
static idryer::ArduinoWifiManager     s_wifi;
static idryer::ArduinoCredentialStore s_credentials;
static idryer::ArduinoHttpClient      s_http;

static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
static idryer::MqttClient               s_mqtt;
static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
static idryer::ActionDispatcher         s_dispatcher;

static MyProfile             s_profile;
static idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
```

### 3. コマンドハンドラーを登録して開始

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    const char* action = data["action"] | "";
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))
    {
        StaticJsonDocument<256> doc;
        s_profile.getConfig(doc);
        s_mqtt.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
}

void setup() {
    Serial.begin(115200);
    idryer::hal::initArduinoHal(&Serial);
    // ... WiFi認証情報を読み込み、seedSerialFromMac ...
    s_runtime.setCommandHandler(handleCommand);
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();
}
```

---

## シナリオ2: MQTT + ローカルWSデバイス

最小限のシナリオを拡張。`LocalAccess` (LAN WebSocket + mDNS) と `DevicePublisher` を追加 — 1回の呼び出しで両方のトランスポートに公開するための薄いラッパー。

リファレンス: [`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### 追加オブジェクト

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### コマンドハンドラー — 両方のトランスポート用

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    const char* action = data["action"] | "";
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))
    {
        StaticJsonDocument<256> doc;
        s_profile.getConfig(doc);
        s_pub.publishConfig(doc);   // → MQTT + WS
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
}
```

### setup()での初期化

```cpp
s_credentials.seedSerialFromMac();
{
    idryer::DeviceIdentity identity;
    s_credentials.load(identity);
    s_local.initMdns(identity.serialNumber);   // WSが開始される前にmDNS
    s_local.begin(identity.serialNumber, identity.token);
    s_local.setCommandSink(handleCommand);     // 同じハンドラー
    s_local.setTokenRefreshCallback([]() {
        idryer::DeviceIdentity id;
        s_credentials.load(id);
        s_local.updateToken(id.token);
    });
}
s_runtime.setCommandHandler(handleCommand);
s_runtime.begin();
```

### loop()

```cpp
void loop() {
    s_runtime.loop();
    s_local.loop();
    // 製品ロジック — センサー、s_pub経由のテレメトリ
}
```

---

## テレメトリ

`s_pub` 経由で定期的にテレメトリを公開します (または最小限のシナリオでは `s_mqtt` 経由で直接):

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

または専用クラスにラップします (例: Storage LinkのStorageTelemetryPublisher)。

## コントラクトを説明する

新しいトピックを追加したりペイロードを変更する場合:

1. `contracts/mqtt_contract.yaml` を更新します。
2. `docs/ru/` に説明を追加します。

## 適用性

現在のモデルは以下に適しています:

- クラウド接続を備えたスタンドアロンデバイス (WiFi + MQTT)
- LAN経由のローカルWSアクセスを備えたデバイス
- NVSメニュー付きの構成可能なデバイス

デュアルMCUデバイス (ESP32 + RP2040) の場合 — UARTブリッジを接続してください (`idryer_uart.h`)。プリンター統合を備えたデバイスの場合 — `idryer_integrations.h`。
