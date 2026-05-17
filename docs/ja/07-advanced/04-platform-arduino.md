# Arduino プラットフォーム

ライブラリはプラットフォームを抽象化するための3つのインターフェースを定義します：

- `IWifiManager` — WiFi 管理。
- `ICredentialStore` — デバイス ID ストレージ。
- `IHttpClient` — HTTP リクエスト。

これらのインターフェースの Arduino 実装は `platform/arduino/` にあります。ESP32/Arduino に対してのみコンパイルされます。

## ArduinoWifiManager

Arduino `WiFi` の上に `IWifiManager` を実装します。

```cpp
class ArduinoWifiManager : public IWifiManager {
    void begin(const char* ssid, const char* password) override;
    bool connect() override;
    bool isConnected() override;
    void disconnect() override;
    void getLocalIP(char* buffer, size_t bufferSize) override;
    void getSSID(char* buffer, size_t bufferSize) override;
    int  getRSSI() override;
    void getMacAddress(char* buffer, size_t bufferSize) override;
    void loop() override;
};
```

`begin()` は認証情報を格納して接続を開始します。複数回呼び出しても安全（例えば、Improv プロビジョニング後）。

`loop()` は `CloudStateMachine::loop()` の内部で呼ばれます。プロダクトは呼ぶ必要はありません。

## ArduinoCredentialStore

ESP32 NVS（`Preferences`）、名前空間 `"idryer"` を通じて `ICredentialStore` を実装します。

3つのフィールドを格納：

| NVS キー | 内容 |
|---------|---------|
| `serial` | デバイス シリアル番号（MQTT ユーザー名） |
| `token` | デバイス トークン（MQTT パスワード） |
| `deviceId` | バックエンド UUID（クレーミング後） |

```cpp
bool load(DeviceIdentity& identity);  // true if token is not empty
bool save(const DeviceIdentity& identity);
void clear();
```

追加メソッド：

```cpp
void seedSerialFromMac();
```

NVS にシリアル番号がない場合、WiFi MAC アドレスから `DEVICE_AABBCCDDEEFF` 形式で生成して保存します。`setup()` で `runtime.begin()` の前に呼び出します。

## ArduinoHttpClient

`WiFiClientSecure` を通じて `IHttpClient` を実装します。

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // default 10000 ms
```

Let's Encrypt ISRG Root X1 ルート CA（`root_ca.h` より）を使用します。`CloudStateMachine` がプロビジョニングとクレーミング ポーリングに使用します。プロダクトは直接呼びません。

## ArduinoWifiStore

別個のクラス（インターフェースを実装しない）で、NVS 内の WiFi 認証情報の格納、名前空間 `"wifi"`。Improv WiFi と共に使用します。

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

`setup()` での典型的な使用：

```cpp
ArduinoWifiStore wifiStore;

// Restore saved credentials
char ssid[64], pass[64];
if (wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
    wifi.begin(ssid, pass);
}

// Save after Improv
improv.onImprovConnected([&](const char* s, const char* p) {
    wifiStore.save(s, p);
    wifi.begin(s, p);
});
```

## HAL：ArduinoTime と ArduinoLogger

`hal/hal_arduino.h` には HAL インターフェースの Arduino 実装が含まれています：

- `ArduinoTime` — `millis()`、`micros()`、`delay()`、`delayMicroseconds()` をデリゲート。
- `ArduinoLogger` — レベルと ANSI 色付きで `Stream` へのフォーマット出力。
- `ArduinoSerial` — `UartBridge` の `HardwareSerial` をラップ。

初期化：

```cpp
// In setup() — logs disabled while Improv owns Serial
idryer::hal::initArduinoHal(nullptr);

// After WiFi connects
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` は安全に呼べます：すべての `HAL_LOG_*` マクロはノーオペになります。

## この抽象化が必要な理由

`CloudStateMachine` は `IWifiManager*` と `ICredentialStore*` を受け入れます。これにより：

- リアル WiFi なしのホスト上でテストを実行（モックで置き換え）。
- 別のプラットフォーム（非 Arduino）をサポート（ライブラリ コアを変更せず）。
- プロビジョニング ロジックをハードウェアから独立してテスト。

実際には、iDryer プロダクトでは Arduino 実装のみが使用されます。
