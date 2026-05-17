# Plataforma Arduino

A biblioteca define três interfaces para abstrair a plataforma:

- `IWifiManager` — gestão de WiFi.
- `ICredentialStore` — armazenamento de identidade do dispositivo.
- `IHttpClient` — requisições HTTP.

Implementações Arduino dessas interfaces estão em `plataforma/arduino/`. Elas são compiladas apenas para ESP32/Arduino.

## ArduinoWifiManager

Implementa `IWifiManager` em cima de Arduino `WiFi`.

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

`begin()` armazena credenciais and inicia conexão. Seguro chamar várias vezes (e.g., após aprovisionamento Improv).

`loop()` é chamado dentro de `CloudStateMachine::loop()`. O produto não precisa to call it.

## ArduinoCredentialStore

Implements `ICredentialStore` via ESP32 NVS (`Preferences`), namespace `"idryer"`.

Armazena três campos:

| NVS key | Content |
|---------|---------|
| `serial` | número de série do dispositivo (nome de utilizador MQTT) |
| `token` | token do dispositivo (senha MQTT) |
| `deviceId` | UUID de backend (após reclamação) |

```cpp
bool load(DeviceIdentity& identity);  // true if token is not empty
bool save(const DeviceIdentity& identity);
void clear();
```

Additional method:

```cpp
void seedSerialFromMac();
```

Se NVS não tiver número de série number — gera um do endereço MAC de WiFi no formato `DEVICE_AABBCCDDEEFF` and o guarda. Chamar em `setup()` antes de `runtime.begin()`.

## ArduinoHttpClient

Implements `IHttpClient` via `WiFiClientSecure`.

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // default 10000 ms
```

Usa o certificado Let's Encrypt ISRG Root X1 CA raiz (de `root_ca.h`). Utilizado por `CloudStateMachine` para aprovisionamento and polling de reclamação. O produto não a chama directamente.

## ArduinoWifiStore

Classe separada (não implementa uma interface) para armazenar WiFi credenciais em NVS, namespace `"wifi"`. Utilizado juntamente com Improv WiFi.

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

Typical usage in `setup()`:

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

## HAL: ArduinoTime and ArduinoLogger

`hal/hal_arduino.h` contains Implementações Arduino de HAL interfaces:

- `ArduinoTime` — delega `millis()`, `micros()`, `delay()`, `delayMicroseconds()`.
- `ArduinoLogger` — saída formatada to `Stream` com níveis and cores ANSI.
- `ArduinoSerial` — envolve `HardwareSerial` for `UartBridge`.

Inicialização:

```cpp
// In setup() — logs disabled while Improv owns Serial
idryer::hal::initArduinoHal(nullptr);

// After WiFi connects
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` é seguro to call: todas as macros `HAL_LOG_*` macros tornam-se sem-ops.

## Por que esta abstração é necessária

`CloudStateMachine` aceita `IWifiManager*` and `ICredentialStore*`. Isso permite:

- Executar testes numa máquina sem WiFi real (substituir por mocks).
- Apoiar outro plataforma (não-Arduino) sem alterar o núcleo da biblioteca.
- Testar aprovisionamento lógica independentemente of hardware.

Na prática, only Implementações Arduino are used em produtos iDryer.
