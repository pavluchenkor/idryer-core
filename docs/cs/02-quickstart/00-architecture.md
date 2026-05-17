# Jak idryer-core funguje

idryer-core je knihovna pro ESP32, která zvládá celý cloud stack: zřizování WiFi přes Improv-Serial, protokol claim pro vazbu zařízení na účet idryer.org, TLS MQTT relaci s auto-reconnectem, směrování příkazů z portálu a periodické publikování telemetrie.

Píšete jen to, co je specifické pro vaše zařízení: čtení senzorů, řízení periférií. Vše ostatní je v knihovně.

## mqtt_contract.yaml — jediný zdroj pravdy

Soubor [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) definuje:

- **capabilities** — jaké periférie jednotlivé typy zařízení podporují (topidlo, LED páska, senzory);
- **telemetry fields** — názvy polí a datové typy v MQTT paketech;
- **UART protocol** — struktury mezi ESP32 a ko-procesorem;
- **TypeScript types** — pro frontend portálu.

Z tohoto souboru se kód generuje automaticky:

| Co je generováno | Kde |
|---|---|
| `iDryer::Config` (has* příznaky) | `src/_generated/iDryer_api.h` |
| MQTT témata (C++ konstanty) | `contracts/_generated/mqtt_topics.h` |
| TypeScript typy | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Neupravujte ručně soubory v `src/_generated/` a `contracts/_generated/` — budou přepsány při příštím spuštění regenerace.

## Jak přidat nové periférie

Postup je stejný pro jakoukoli novou schopnost — tlačítko, senzor CO2, čtecí zařízení RFID.

**1.** Přidejte záznam do `capability_vocabulary` v [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml):

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** Spusťte regeneraci:

```bash
cd contracts
./regen.sh
```

Poté bude `iDryer::Config` mít pole `hasCo2` a TypeScript bude mít `HardwareUnitConfigCapabilities.co2`.

**3.** Nastavte příznak v `main.cpp` vašeho zařízení:

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Nahrajte zařízení. Portál přečte `co2: true` z MQTT tématu `/info` a automaticky zobrazí odpovídající blok UI — nejsou potřebné žádné změny na straně portálu.

U typů periférií, které v kontraktu ještě nejsou, otevřete PR do repozitáře idryer-core s přidáním záznamu do `capability_vocabulary`. Po sloučení — spusťte `regen.sh`.

## Dva produkční produkty postavené na této knihovně

**iDryer Storage Link** — ESP32-C3 s WS2812B LED páskou a senzorem teploty/vlhkosti SHT31.

**iHeater Link** — ESP32-C3 s RMT výstupem na topidlo iHeater, s integrací pro Bambu Lab, Klipper/Moonraker a Home Assistant.

Oba produkty obsahují idryer-core přes PlatformIO `lib_deps` a implementují pouze svou logiku specifickou pro produkt.

## Co dál

- [01-wifi.md](01-wifi.md) — připojte ESP32 k WiFi pomocí Improv-Serial.
- [../../../README.md](../../../README.md) — přehled knihovny a odkaz na generování kódu.
