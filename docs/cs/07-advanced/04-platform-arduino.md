# Platforma Arduino

Knihovna definuje tři rozhraní pro abstrakci platformy:

- `IWifiManager` — správa WiFi.
- `ICredentialStore` — úložiště identity zařízení.
- `IHttpClient` — HTTP požadavky.

Implementace Arduino těchto rozhraní jsou v `platform/arduino/`. Jsou kompilovány pouze pro ESP32/Arduino.

## ArduinoWifiManager

Implementuje `IWifiManager` na základě Arduino `WiFi`.

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

`begin()` ukládá přihlašovací údaje a iniciuje připojení. Bezpečné volání vícekrát (např. po zřizování Improv).

`loop()` se volá uvnitř `CloudStateMachine::loop()`. Produkt jej nemusí volat.

## ArduinoCredentialStore

Implementuje `ICredentialStore` prostřednictvím ESP32 NVS (`Preferences`), obor názvů `"idryer"`.

Ukládá tři pole:

| NVS klíč | Obsah |
|---------|---------|
| `serial` | sériové číslo zařízení (jméno uživatele MQTT) |
| `token` | token zařízení (heslo MQTT) |
| `deviceId` | UUID backendu (po požadavku) |

```cpp
bool load(DeviceIdentity& identity);  // true, pokud token není prázdný
bool save(const DeviceIdentity& identity);
void clear();
```

Dodatečná metoda:

```cpp
void seedSerialFromMac();
```

Pokud NVS nemá sériové číslo — vygeneruje jedno z adresy WiFi MAC ve formátu `DEVICE_AABBCCDDEEFF` a uloží jej. Zavolejte v `setup()` před `runtime.begin()`.

## ArduinoHttpClient

Implementuje `IHttpClient` prostřednictvím `WiFiClientSecure`.

```cpp
bool postJson(const char* url, const char* body, JsonDocument& response) override;
bool getJson(const char* url, JsonDocument& response) override;
void setTimeout(uint32_t timeoutMs) override; // výchozí 10000 ms
```

Používá kořenový certifikát Let's Encrypt ISRG Root X1 (z `root_ca.h`). Používá se v `CloudStateMachine` pro zřizování a zjišťování požadavků. Produkt jej nevolá přímo.

## ArduinoWifiStore

Samostatná třída (neimplementuje rozhraní) pro ukládání přihlašovacích údajů WiFi v NVS, obor názvů `"wifi"`. Používá se společně s Improv WiFi.

```cpp
bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);
void save(const char* ssid, const char* password);
```

Typické použití v `setup()`:

```cpp
ArduinoWifiStore wifiStore;

// Obnovte uložené přihlašovací údaje
char ssid[64], pass[64];
if (wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
    wifi.begin(ssid, pass);
}

// Uložte po Improve
improv.onImprovConnected([&](const char* s, const char* p) {
    wifiStore.save(s, p);
    wifi.begin(s, p);
});
```

## HAL: ArduinoTime a ArduinoLogger

`hal/hal_arduino.h` obsahuje implementace Arduino rozhraní HAL:

- `ArduinoTime` — delegáty `millis()`, `micros()`, `delay()`, `delayMicroseconds()`.
- `ArduinoLogger` — formátovaný výstup do `Stream` s úrovněmi a ANSI barvami.
- `ArduinoSerial` — zabaluje `HardwareSerial` pro `UartBridge`.

Inicializace:

```cpp
// V setup() — protokoly jsou zakázány, zatímco Improv vlastní Serial
idryer::hal::initArduinoHal(nullptr);

// Po připojení WiFi
idryer::hal::initArduinoHal(&Serial);
```

`initArduinoHal(nullptr)` je bezpečné zavolat: všechna makra `HAL_LOG_*` se stanou no-ops.

## Proč je tato abstrakce potřebná

`CloudStateMachine` přijímá `IWifiManager*` a `ICredentialStore*`. To umožňuje:

- Spouštění testů na počítači bez skutečného WiFi (nahrazuje se mock objekty).
- Podpora jiné platformy (ne-Arduino) bez změny jádra knihovny.
- Testování logiky zřizování nezávisle na hardwaru.

V praxi se používají pouze implementace Arduino v produktech iDryer.
