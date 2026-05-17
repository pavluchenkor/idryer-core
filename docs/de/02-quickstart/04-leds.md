# Schritt 04 — Anzeige: LED-Streifen gesteuert durch Sensordaten

Nach diesem Schritt ändert ein WS2812B-Streifen die Farbe basierend auf der Luftfeuchtigkeit, und die Helligkeit kann vom Portal über einen `set` Befehl gesteuert werden.

## Was Sie benötigen

**Hardware:**

- WS2812B LED-Streifen (oder WS2811/SK6812)
- 330–470 Ω Widerstand auf der Datenleitung
- 5 V Stromversorgung (Strom hängt von der Streifenlänge ab; 300 LEDs verbrauchen bis zu 18 A)

**Software:**

- Bibliothek `fastled/FastLED @ ^3.6.0`

!!! warning
    Speisen Sie den Streifen von einer dedizierten 5 V Stromversorgung. Das Speisen über den 3,3 V oder 5 V Pin des Boards ist nur für einen schnellen Rauchtest mit wenigen LEDs akzeptabel.

## Schritte

**1. Fügen Sie FastLED** zu `platformio.ini` hinzu:

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... andere Abhängigkeiten
```

**2. Deklarieren Sie den Puffer und Executor** in `main.cpp`. Basierend auf [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp
#include <FastLED.h>
#include "storage/led_strip/led_strip_executor.h"

#define STORAGE_LED_PIN  4
#define STORAGE_MAX_LEDS 300

static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. Initialisieren Sie den Streifen** in `setup()`:

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

Ersetzen Sie `60` durch die tatsächliche LED-Anzahl Ihres Streifens.

**4. Ändern Sie die Farbe nach Luftfeuchtigkeit** in `loop()`. Farbskala: blau (trocken) → gelb → rot (feucht):

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airHumidityPct[0] = r.humidity;

        // Humidity 20%–80% → hue from 160 (blue) to 0 (red).
        float h = constrain(r.humidity, 20.0f, 80.0f);
        uint8_t hue = (uint8_t)(160.0f - (h - 20.0f) / 60.0f * 160.0f);
        fill_solid(s_leds, s_executor.ledsCount(), CHSV(hue, 255, 200));
        FastLED.show();
    }
}
```

**5. Steuern Sie die Helligkeit vom Portal.** Registrieren Sie einen `set` Befehls-Handler in `setup()`:

```cpp
s_link.onCommand("set", [](JsonObjectConst data) {
    int id  = data["id"]  | -1;
    int val = data["val"] | -1;
    if (id == MENU_BRIGHTNESS && val >= 0 && val <= 255) {
        FastLED.setBrightness((uint8_t)val);
        FastLED.show();
    }
});
```

`MENU_BRIGHTNESS` ist eine Konstante aus [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h), generiert von `menu.yaml` über `regen.sh`. In Ihrem eigenen Produkt unterscheiden sich Name und Wert — überprüfen Sie `menu_ids.h` Ihres Projekts.

## Überprüfung

Nach dem Flashen sollte der Streifen in der Farbe aufleuchten, die der aktuellen Luftfeuchtigkeit entspricht. Wenn kein Sensor vorhanden ist, bleibt der Streifen aus (Executor erhält keine Daten).

Öffnen Sie die Geräteeinstellungen im Portal und passen Sie den Helligkeitsschieber an — der Streifen reagiert sofort.

## Nächste Schritte

- [05-rmt-command.md](05-rmt-command.md) — steuern Sie einen Aktuator von einem Portal-Befehl (RMT-Ausgang).
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — Executor-API: Zonen-Puls, Animationen, Helligkeit.
