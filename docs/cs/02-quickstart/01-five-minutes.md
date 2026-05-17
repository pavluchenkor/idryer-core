# Začněte za 5 minut

Po této stránce bude váš ESP32 nahrán, připojí se k WiFi a zobrazí se na [portal.idryer.org](https://portal.idryer.org/) se stavem Online. Požadavky: ESP32-C3 (DevKit, Super Mini nebo kompatibilní), USB kabel, PlatformIO ve VS Code.

## 1. Připravte secrets.h

Zkopírujte [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) do `include/secrets.h` ve vašem projektu a nastavte vaše SSID a heslo WiFi (pouze 2.4 GHz):

```cpp
#define WIFI_SSID      "vaše-ssid"
#define WIFI_PASSWORD  "vaše-heslo"
```

Přidejte `include/secrets.h` do `.gitignore`.

## 2. Nakonfigurujte platformio.ini

Vytvořte `platformio.ini` v kořenu projektu:

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://cesta/k/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

Změňte `board` tak, aby odpovídal vaší desce. Nahraďte `cesta/k/idryer-core` skutečnou cestou ke knihovně.

## 3. Zkopírujte příklad 01_blink_status

Zkopírujte obsah [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) do `src/main.cpp` vašeho projektu. Příklad nevyžaduje žádné senzory ani další závislosti — pouze minimální kořen kompozice.

## 4. Nahrajte

```bash
pio run -e blink-demo -t upload
```

## 5. Otevřete Serial Monitor

```bash
pio device monitor -b 115200
```

Očekávaná posloupnost logu:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

Po zadání PIN kódu na portálu (krok 6):

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Pokud se zařízení zastavilo na zprávě `PIN: ...` — to je očekávané; pokračujte na krok 6.

## 6. Deklarujte zařízení na portálu

Otevřete [portal.idryer.org](https://portal.idryer.org/), přejděte na **Add device** a zadejte PIN z Serial Monitor. Po úspěšné deklaraci se zařízení přepne na `Online` a vestavěná LED bude blikat každých 500 ms.

Podrobný tok deklarace: [Onboarding](02-onboarding.md).

## Co dál

- Přidejte senzor — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- Přidejte periférii — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- Úplný návod API — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Jak to funguje vnitřně — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
