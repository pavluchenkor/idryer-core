# Veröffentlichung über devicePublisher

## Wann man dies verwenden sollte

`iDryer::Link` enthält bereits zwei eingebaute Transporte: MQTT (Cloud) und Local WebSocket (LAN). Ein zusätzlicher Transport ist für die meisten Aufgaben nicht erforderlich.

Verwenden Sie `s_link.devicePublisher()`, wenn das Produkt sein eigenes Payload zusammensetzt und es an beide Kanäle gleichzeitig senden muss — beispielsweise beim Veröffentlichen der Menükonfiguration in Antwort auf `commands/get_config`.

## Schlüsselfertig-Code

```cpp
// main.cpp (fragment)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// Publish an arbitrary JSON payload to MQTT and Local WS in a single call.
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // product function
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

Ein einzelner `publishConfigRaw` Aufruf liefert die Payload an das MQTT-Thema `idryer/{serial}/config` und an alle aktiven LAN-WS-Clients. Es müssen keine zusätzlichen Clients oder Themen erstellt werden.

## Erklärung

`devicePublisher()` ist der Hilfshelferer der Fassade für Dual-Publishing. Verwenden Sie ihn anstelle von direkten `mqttClient()` oder `LocalAccess` Aufrufen, es sei denn, Sie müssen zu einem nicht standardisierten Thema veröffentlichen.

Telemetrie und Status werden automatisch von der Fassade nach einem Timer veröffentlicht — `devicePublisher()` ist für diese nicht erforderlich.

## Wenn ein dritter Transport benötigt wird

Das Hinzufügen eines dritten Kanals (BLE, Serial JSON, UART Proxy) ist eine architektonische Erweiterung der Fassade, nicht ein Rezeptmuster. Die überwiegende Mehrheit der Geräte benötigt dies nicht.

Falls Sie es brauchen — Einstiegspunkte befinden sich in `lib/idryer-core/src/cloud/` (Cloud State Machine, MQTT) und `lib/idryer-core/src/` (lokaler Zugriff). Bestätigen Sie vor dem Fortfahren, dass die eingebauten MQTT und Local WS für Ihren Anwendungsfall unzureichend sind.

## Vollständiges Beispiel im Repo

`publishFullMenu()` in `iDryer-Storage/src/main.cpp:171` — Veröffentlichung des vollständigen JSON-Menüs über `s_link.devicePublisher()->publishConfigRaw(buf, len)`.
