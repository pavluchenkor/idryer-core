---
title: "Spustit zařízení na idryer-core za 5 minut"
description: "První zařízení na idryer-core: projekt PlatformIO, minimální firmware, Wi-Fi a spárování s účtem v aplikaci iDryer."
---

# Spustit zařízení na idryer-core za 5 minut

Po této stránce bude ESP32 v síti, spárovaný s vaším účtem a viditelný na portálu [portal.idryer.org](https://portal.idryer.org/) i v aplikaci iDryer. Budete potřebovat: desku ESP32-C3 (DevKit, Super Mini nebo kompatibilní), USB kabel, PlatformIO ve VS Code, telefon s aplikací iDryer, síť Wi-Fi 2,4 GHz.

## 1. Projekt PlatformIO

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← kopie, git submodul nebo symbolický odkaz
└── src/
    └── main.cpp
```

`platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP je transport ESP8266 ze závislostí espMqttClient: na ESP32 se nesestaví.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Deska bez USB-UART (ESP32-C3 SuperMini): Serial přes USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

Knihovny jádra (MQTT, ArduinoJson, WebSockets, Improv) přijdou samy z `library.json` jádra. Deska s USB-UART poslední dva příznaky nepotřebuje; `board` nastavte podle své desky.

## 2. Kód

Zkopírujte do `src/main.cpp` příklad [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino):

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // vlastní zařízení
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Perioda blikání podle stavu; 0 = neblikat.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Odpojení: jádro smaže tajemství a znovu čeká na spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // Pole telemetrie jádro publikuje samo, každých 30 s (v klidu každých 60 s).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

Ve firmwaru není heslo k síti ani údaje účtu: síť a spárování zařízení dostane z aplikace.

## 3. Nahrát a otevřít log

```bash
pio run -t upload
pio device monitor -b 115200
```

Dokud zařízení nemá Wi-Fi, log mlčí: jádro drží port pro Improv. LED bliká rychle: zařízení čeká na síť a spárování.

## 4. Wi-Fi a spárování v aplikaci

1. Připojte telefon k síti Wi-Fi, ve které bude zařízení pracovat, a přihlaste se do aplikace iDryer svým účtem portálu.
2. Na hlavní obrazovce klepněte na **Připojit nové zařízení**: otevře se krok **Wi-Fi**.
3. Zkontrolujte název sítě, zadejte heslo a klepněte na **Připojit zařízení**. Aplikace posílá nastavení až 90 sekund; když se zařízení připojí, zobrazí se **Zařízení připojeno**. Klepněte na **Další**.
4. V kroku **Spárování** klepněte na **Spárovat**. Aplikace najde zařízení v síti, získá od portálu jednorázový párovací token a předá ho zařízení.
5. Po zprávě **Zařízení spárováno** se zařízení objeví v seznamu zařízení na portálu i v aplikaci.

Log po připojení k síti:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Po spárování:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Kontrola

- zařízení je online na portálu i v aplikaci;
- karta ukazuje teplotu čipu ESP32;
- LED bliká jednou za sekundu. Trvale svítí nebo nesvítí: není spojení s portálem.

## Když to nevyšlo

- aplikace se nedočkala připojení zařízení: zkontrolujte heslo a že síť je 2,4 GHz; při špatném hesle zařízení znovu čeká na nastavení. Další způsoby, jak předat síť: [Wi-Fi](01-wifi.md);
- v kroku **Spárování** se zařízení nenašlo: telefon a zařízení musí být ve stejné síti; sítě pro hosty často blokují vyhledávání zařízení. Více: [Spárování s účtem](02-claim.md);
- sestavení a příznaky: [Podrobné nastavení](99-detailed-setup.md).

## Dál

- [Telemetrie](03-telemetry.md): senzor a vlastní veličina na kartě.
- [Příklady jádra](https://github.com/pavluchenkor/idryer-core/tree/main/examples): akce karty, vlastní senzory a ovládací prvky.
