# Arduino-Plattform

Die Bibliothek definiert drei Schnittstellen zur Abstrahierung der Plattform:

- `IWifiManager` — WiFi-Verwaltung.
- `ICredentialStore` — Speicherung der Geräteidentität.
- `IHttpClient` — HTTP-Anfragen.

Arduino-Implementierungen dieser Schnittstellen befinden sich in `platform/arduino/`. Sie werden nur für ESP32/Arduino kompiliert.

## ArduinoWifiManager

Implementiert `IWifiManager` auf Basis des Arduino `WiFi`.

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

`begin()` speichert Anmeldedaten und leitet die Verbindung ein. Sicher mehrmals aufrufbar (z.B. nach Improv-Provisioning).

`loop()` wird innerhalb von `CloudStateMachine::loop()` aufgerufen. Das Produkt braucht es nicht aufzurufen.

## ArduinoCredentialStore

Implementiert `ICredentialStore` über ESP32 NVS (`Preferences`), Namespace `"idryer"`.

Speichert drei Felder:

| NVS-Schlüssel | Inhalt |
|---------|---------|
| `serial` | Geräteseriennummer (MQTT-Benutzername) |
| `token` | Gerätetoken (MQTT-Passwort) |
| `deviceId` | Backend-UUID (nach dem Claim) |

```cpp
bool load(DeviceIdentity& identity);  // true if token is not empty
bool save(const DeviceIdentity& identity);
void clear();
```

Zusätzliche Methode:

```cpp
void seedSerialFromMac();
```

Wenn NVS keine Seriennummer hat — generiert eine aus der WiFi-MAC-Adresse im Format `DEVICE_AABBCCDDEEFF` und speichert sie. Rufen Sie in `setup()` vor `runtime.begin()` auf.

## ArduinoHttpClient

Implementiert `IHttpClient` über `WiFiClientSecure`.

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // default 10000 ms
```

Verwendet das Let's Encrypt ISRG Root X1 Root-Zertifikat (von `root_ca.h`). Wird von `CloudStateMachine` für Provisioning und Claim-Abfrage verwendet. Das Produkt ruft es nicht direkt auf.

## ArduinoWifiStore

Separate Klasse (implementiert keine Schnittstelle) zum Speichern von WiFi-Anmeldedaten in NVS, Namespace `"wifi"`. Wird zusammen mit Improv WiFi verwendet.

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

Typische Verwendung in `setup()`:

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

## HAL: ArduinoTime und ArduinoLogger

`hal/hal_arduino.h` enthält Arduino-Implementierungen von HAL-Schnittstellen:

- `ArduinoTime` — delegiert `millis()`, `micros()`, `delay()`, `delayMicroseconds()`.
- `ArduinoLogger` — formatierte Ausgabe zu `Stream` mit Stufen und ANSI-Farben.
- `ArduinoSerial` — umwickelt `HardwareSerial` für `UartBridge`.

Initialisierung:

```cpp
// In setup() — logs disabled while Improv owns Serial
idryer::hal::initArduinoHal(nullptr);

// After WiFi connects
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` ist sicher aufzurufen: Alle `HAL_LOG_*` Makros werden zu No-Ops.

## Warum diese Abstraktion benötigt wird

`CloudStateMachine` akzeptiert `IWifiManager*` und `ICredentialStore*`. Dies ermöglicht:

- Tests auf einem Host ohne echtes WiFi (ersetzen durch Mocks).
- Unterstützung einer anderen Plattform (Nicht-Arduino) ohne Änderung des Bibliothekskerns.
- Testen von Provisioning-Logik unabhängig von Hardware.

In der Praxis werden nur Arduino-Implementierungen in iDryer-Produkten verwendet.
