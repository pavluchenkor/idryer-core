---
title: "Jak přidat nový produkt založený na idryer-core"
description: "Kontrolní seznam nového zařízení iDryer na fasádě iDryer::Link: projekt, minimální firmware, Wi-Fi a spárování v aplikaci, telemetrie, menu nastavení, karta zařízení, kontrakt."
---

# Jak přidat nový produkt založený na idryer-core

Tento návod použijte, když stavíte nový produkt na `idryer-core`: sušičku filamentu, topný blok, osvětlení, senzor nebo jiný modul. Ukazuje, co za vás udělá jádro a co musí doplnit kód produktu.

Úplný sestavitelný příklad je vyhřívaná skříň pro skladování z dokumentace Build-Your-Own-iDryer (`example/09-cabinet`): prochází vším, co je na této stránce.

---

## Na čem produkt stojí

Produkt komunikuje s jádrem přes jediný objekt — fasádu `iDryer::Link` (`<iDryer.h>`). Uvnitř `s_link.begin()` a `s_link.loop()` jádro:

- získá síť Wi-Fi z aplikace iDryer vzduchem (ESPTouch) nebo z webového instalátoru přes USB (Improv) a udržuje spojení;
- propojí zařízení s účtem: čeká na jednorázový párovací token — z aplikace po místní síti nebo přes sériový port (`PAIR_TOKEN:<token>`) — a na portálu ho vymění za trvalý tajný klíč;
- připojí se k MQTT a publikuje `telemetry` a `status` v periodách z `Config`;
- ohlásí se v místní síti (mDNS `_idryer._tcp`) a přijímá příkazy z aplikace přes WebSocket;
- publikuje card manifest karty zařízení.

Třídy nižší úrovně (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` a další) jsou vnitřnosti jádra: párovací token se ke cloudové části dostane jen uvnitř `iDryer::Link`. Stavte produkt na fasádě.

---

## 1. Projekt

`idryer-core` patří do `lib/idryer-core/` (kopie nebo symbolický odkaz); knihovny jádra si PlatformIO vezme z jeho `library.json`. Minimální `platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; transport ESP8266 ze závislostí espMqttClient: na ESP32 se nesestaví
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Bez `lib_ignore = ESPAsyncTCP` sestavení spadne v `ESPAsyncTCP.cpp`; bez `MQTT_BROKER` a `MQTT_PORT` se jádro nezkompiluje.

---

## 2. Minimální firmware

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // není produkt iDryer: karta vzniká z manifestu
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // vaše funkce senzorů
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` spustí Wi-Fi, propojení, MQTT a místní přístup; `s_link.loop()` musí běžet neustále, bez `delay()`. Příkaz `revoke` přijde z portálu, když zařízení od účtu odpojíte: `handleRevoke()` smaže tajný klíč a zařízení čeká na nový párovací token.

---

## 3. Wi-Fi a propojení — nic v kódu

Firmware neobsahuje heslo k síti ani údaje účtu. Uživatel zařízení připojí v aplikaci iDryer: **Připojit nové zařízení** → krok **Wi-Fi** (aplikace pošle síť přes ESPTouch) → krok **Spárování** (aplikace najde zařízení přes mDNS, získá od portálu jednorázový token a předá ho zařízení; token na portálu aktivuje zařízení samo). Dokud Wi-Fi nenaběhne, sériový port mlčí: jádro ho drží pro webový instalátor (Improv).

Krok za krokem a s očekávaným logem — Build-Your-Own-iDryer, kapitola „Spuštění firmwaru na jádře“.

---

## 4. Data: telemetrie a stav

- Příznaky `has*` v `Config` určují, která slovníková pole jdou do telemetrie a které buňky se objeví na kartě.
- Hodnoty zapisujte do `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) a `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); jádro je publikuje v periodách z `Config` a `s_link.publishStatusNow()` pošle stav hned.
- Vlastní pole — přes `s_link.onTelemetryPublish()`, na kartu — přes `s_link.card().sensor()`; viz [Karta zařízení](02-add-widget.md).

---

## 5. Nastavení: menu

Nastavení se popisují v `src/menu/menu.yaml`; generátor `menu_gen.py` z něj udělá kód C++, uložení v NVS a JSON menu ([Menu jako protokol](../08-contracts/02-menu-as-protocol.md)). Kód produktu:

- načte menu před `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- publikuje ho přes `menu_buildFullJson()` a `s_link.devicePublisher()->publishConfigRaw()` — po přechodu online a na příkaz `get_config`;
- aplikuje příkaz `set` přes `menu_apply_by_bind()` (hodnota, NVS i cache najednou) a publikuje menu znovu.

Úplný kód — Build-Your-Own-iDryer, kapitola „Menu z YAML“.

---

## 6. Karta zařízení

Co karta ukazuje a jaké operace spouští, se deklaruje přes `s_link.card()` — [Karta zařízení: card manifest](02-add-widget.md).

---

## 7. Kontrakt

Když přidáváte nové topicy nebo měníte payload:

1. aktualizujte `contracts/mqtt_contract.yaml`;
2. spusťte `contracts/regen.sh` a commitněte vygenerované soubory.

---

## Dvoučipová zařízení

Pro ESP32, které pracuje se samostatným řadičem (například RP2040) přes UART, má jádro most UART `idryer_uart.h`; funkční vzor je firmware `idryer-link`.
