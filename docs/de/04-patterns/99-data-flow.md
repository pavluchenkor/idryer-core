# Datenfluss zwischen Teilnehmern

Angewandte Sektion: Wie Sensoren, Peripheriegeräte, Profil, Transporte und Publisher in echtem Produktcode verbunden sind. Die architektonische Datenfluss-Beschreibung ist in [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md).

## Prinzip

`idryer-core` bietet absichtlich keinen internen Event Bus. Alle Verbindungen zwischen Teilnehmern sind **explizite Zeiger**, die durch Konstruktoren in der Composition Root weitergeleitet werden. Dies bedeutet:

- Jeder Datenfluss kann als eine Zeigerkette in `main.cpp` gelesen werden.
- Keine "magische" Teilnehmerentdeckung.
- Das Produkt entscheidet, wer was an wen übergibt.

## Typische Verbindungskarte für Storage Link

```
   Sensor (Sht31ClimateSensor)
        │
        │ tick(now), get()
        ▼
   StorageTelemetryPublisher    ──→  DevicePublisher  ──→  MqttClient + LocalAccess
                                                            │
                                                            ▼
                                                       broker / WS-client


   handleCommand   ←──  IdryerRuntime   ←──  MqttClient (commands/*)
        │           ←──  LocalAccess    ←──  WS-client (envelope)
        │
        ├──→  ActionDispatcher  ──→  LedStripExecutor (peripheral)
        ├──→  IProfile::getConfig  ──→  DevicePublisher::publishConfig
        └──→  IProfile::applyConfig (via onSetCommand)
```

Jeder Pfeil ist eine Zeiger-Weiterleitungszeile in `main.cpp`. Beispiel:

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     publisher
```

## Rezept 1 — Sensor veröffentlicht in die Cloud

**Ziel**: Temperatursensor → MQTT.

```
Sensor → Publisher → DevicePublisher → MqttClient + LocalAccess
```

```cpp
static MySensor              s_sensor;
static MyTelemetryPublisher  s_telemetry(&s_sensor, &s_pub);

void loop() {
    s_runtime.loop();
    s_local.loop();
    s_sensor.tick(millis());
    s_telemetry.loop(millis());
}
```

`MyTelemetryPublisher::loop` entscheidet, wann veröffentlicht wird (nach Intervall). Siehe [01-add-sensor.md](01-add-sensor.md).

## Rezept 2 — Cloud-Befehl → Peripheriegerät

**Ziel**: `commands/invoke {"action":"led.pulse",...}` → LED einschalten.

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onInvoke → LedStripExecutor
```

```cpp
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    return s_executor.execute(action, args);
}

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    // ...
}

void setup() {
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_runtime.setCommandHandler(handleCommand);
    // ...
}
```

Siehe [02-add-peripheral.md](02-add-peripheral.md).

## Rezept 3 — LAN-App-Befehl → Peripheriegerät (gleicher Pfad)

**Ziel**: WS-Client auf LAN sendet `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → die gleiche LED schaltet sich ein.

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

Kein neuer Code erforderlich — `s_local.setCommandSink(handleCommand)` mergt bereits beide Transporte in einen Handler.

## Rezept 4 — Sensor → Peripheriegerät (interner Loop)

**Ziel**: Sensor liest Luftfeuchtigkeit → Wenn über Schwellenwert, Ventilator schaltet sich ein.

Dies ist interne Produktlogik; `idryer-core` hat keine API dafür. Machen Sie es direkt:

```cpp
class HumidityController {
public:
    HumidityController(IClimateSensor* sensor, Fan* fan, float threshold)
        : sensor_(sensor), fan_(fan), threshold_(threshold) {}

    void loop(uint32_t nowMs) {
        if (nowMs - lastCheckMs_ < 5000) return;
        lastCheckMs_ = nowMs;

        SensorReading r = sensor_->get();
        if (!r.ok) return;
        if (r.humidity > threshold_)  fan_->on();
        else                          fan_->off();
    }
private:
    IClimateSensor* sensor_;
    Fan*    fan_;
    float           threshold_;
    uint32_t        lastCheckMs_ = 0;
};
```

Verbindung in der Composition Root:

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` kennt diese Klasse nicht und sollte nicht.

## Rezept 5 — Konfigurationsänderung → Peripheriegerät-Reinitialisierung

**Ziel**: Backend sendet `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → LED-Helligkeit ändert sich sofort.

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Peripheral
```
