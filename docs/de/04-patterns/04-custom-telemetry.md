# Benutzerdefinierte Telemetrie (produktspezifische Payload)

## Wann man dies verwenden sollte

idryer-core's Standard-Telemetrie veröffentlicht nur die in dem gemeinsamen Vertrag definierten Felder (`units[].temperature`, `humidity`, `heaterPower` usw.). Wenn Ihr Produkt Top-Level-JSON-Felder hinzufügen muss (z.B. `outputMode`, `targetTempC`, `active`) oder Daten enthalten muss, die nicht in der `Telemetry` Struktur vorhanden sind, verwenden Sie dieses Rezept.

Ein typischer Fall: iHeater Link veröffentlicht `outputMode` und `targetTempC` neben dem Standard `units[]`, damit das Backend `heaterIntent` über das `telemetry:update` WebSocket-Ereignis an das Frontend weiterleiten kann.

## Schritt 1 — Auto-Publish deaktivieren

Setzen Sie `telemetryPeriodMs = 0` in `Config`. Dies verhindert, dass idryer-core eine abgespeckte Payload von selbst veröffentlicht:

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // publish manually
    .statusPeriodMs    = 5000,
};
```

## Schritt 2 — Veröffentlichungsfunktion schreiben

Verwenden Sie `device().mqttClient()->publishTelemetry(doc)`. Binden Sie alle Felder ein, die das Backend erwartet: sowohl produktspezifische (Top-Level) als auch den Standard `units[]` Block.

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Current hardware output intent
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Active integration ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Product-specific top-level fields
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Standard units[] block — backend stores history from this
    // temperature/humidity = 0 if the device has no sensors
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // timestamp is added automatically
}
```

## Schritt 3 — Vom `loop()` aufrufen

```cpp
void loop() {
    device().loop();

    static uint32_t s_lastTelMs = 0;
    if ((uint32_t)(millis() - s_lastTelMs) >= 5000u) {
        s_lastTelMs = millis();
        publishCustomTelemetry();
    }
    // ...
}
```

## Was man nicht tun sollte

- **Veröffentlichen Sie nicht beide** idryer-core Auto-Telemetrie (nicht-null `telemetryPeriodMs`) und benutzerdefinierte Telemetrie gleichzeitig. Das Backend empfängt zwei Nachrichten zum gleichen Thema und verarbeitet beide — Daten werden dupliziert.
- **Rufen Sie nicht `device().publishTelemetryNow()`** auf, wenn `telemetryPeriodMs = 0` — es veröffentlicht die Standard-Stripped-Payload ohne Ihre produktspezifischen Felder.

## Warum die Bibliothek dies nicht selbst tut

idryer-core veröffentlicht bereits `heaterPower: 1` innerhalb von `units[]` — formal ausreichend, um zu wissen, dass die Heizung aktiv ist. Das Problem liegt nicht in der Bibliothek, sondern im Backend (`telemetry.handler.ts`): Es sucht speziell nach einem Top-Level `outputMode` Feld und leitet `heaterIntent` nicht vom Standard `heaterPower` ab. Dies ist technische Schuld auf der Backend-Seite.

Das aktuelle Rezept ist eine vorübergehende Workaround. Wenn das Backend so behoben wird, dass `heaterIntent` von `units[0].heaterPower` abgeleitet wird, können Sie zu `telemetryPeriodMs = 5000` zurückkehren und `publishCustomTelemetry()` entfernen — die Standard-Bibliotheks-Telemetrie funktioniert ohne Änderungen.

Achten Sie auf Updates zu `telemetry.handler.ts`: Sobald dort ein Fallback auf `heaterPower` hinzugefügt ist, wird dieses Rezept überflüssig.
