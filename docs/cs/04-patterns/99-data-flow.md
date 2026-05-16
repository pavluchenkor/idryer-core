# Tok dat mezi účastníky

Aplikovaná sekce: jak jsou senzory, periférie, profil, transporty a vydavatelé propojeni v reálném produktovém kódu. Architektonický popis toku dat je v [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md).

## Princip

`idryer-core` záměrně neposkytuje vnitřní event bus. Všechna spojení mezi účastníky jsou **explicitní ukazatele** předávané konstruktory v kořeni kompozice. To znamená:

- Jakýkoli tok dat lze číst jako řetězec ukazatelů v `main.cpp`.
- Žádné "kouzlo" v objevování účastníků.
- Produkt rozhoduje kdo předává co komu.

## Typická mapa spojení pro Storage Link

```
   Senzor (Sht31ClimateSensor)
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
        ├──→  ActionDispatcher  ──→  LedStripExecutor (periférie)
        ├──→  IProfile::getConfig  ──→  DevicePublisher::publishConfig
        └──→  IProfile::applyConfig (via onSetCommand)
```

Každá šipka je jeden řádek předávání ukazatelů v `main.cpp`. Například:

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            senzor     vydavatel
```

## Návod 1 — Senzor publikuje do cloudu

**Cíl**: teplotní senzor → MQTT.

```
Senzor → Vydavatel → DevicePublisher → MqttClient + LocalAccess
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

`MyTelemetryPublisher::loop` rozhoduje kdy publikovat (podle intervalu). Viz [01-add-sensor.md](01-add-sensor.md).

## Návod 2 — Cloud příkaz → periférie

**Cíl**: `commands/invoke {"action":"led.pulse",...}` → zapnout LED.

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

Viz [02-add-peripheral.md](02-add-peripheral.md).

## Návod 3 — LAN app příkaz → periférie (stejná cesta)

**Cíl**: WS klient na LAN pošle `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → stejná LED se zapne.

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

Není potřeba nový kód — `s_local.setCommandSink(handleCommand)` již spojuje oba transporty do jedné obsluhy.

## Návod 4 — Senzor → Periférie (interní smyčka)

**Cíl**: senzor čte vlhkost → pokud je nad prahem, ventilátor se zapne.

Toto je interní logika produktu; `idryer-core` nemá API pro taková spojení. Udělej to přímo:

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

Propojení v kořeni kompozice:

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` o této třídě nic neví a neměl by.

## Návod 5 — Změna konfigurace → reinicializace periférie

**Cíl**: backend pošle `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → jas LED se změní okamžitě.

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Periférie
```

```cpp
class MyProfile : public idryer::IProfile {
public:
    MyProfile(MyDevice* a) : device_(a) {}

    bool applyConfig(int id, int val) override {
        if (id == CFG_BRIGHTNESS) {
            menu.brightness = val;
            menu.saveToNVS();
            device_->setBrightness(val);   // okamžitá aplikace
            return true;
        }
        return false;
    }
    // ...
private:
    MyDevice* device_;
};
```

Spojení `profil → periférie` je postaveno v kořeni kompozice:

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## Návod 6 — Nová událost → topics událostí

**Cíl**: periférie zachytí chybu → event v `idryer/{serial}/events`.

Periférie nepublikuje sama. Notifikuje produkt; produkt publikuje:

```cpp
class MyDevice {
public:
    using ErrorCallback = std::function<void(int errCode, const char* msg)>;
    void setErrorCallback(ErrorCallback cb) { errCb_ = cb; }
    // ...
private:
    ErrorCallback errCb_;
    void reportError(int code, const char* msg) {
        if (errCb_) errCb_(code, msg);
    }
};

// v main.cpp
s_device.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

Alternativně může periférie přijmout `DevicePublisher*` skrze svůj konstruktor. Klíčový bod: spojení je explicitní.

## Co my nedělamo

- Nezavádíme vnitřní event bus. To by vedlo k skrytým spojením a složitosti ladění.
- Neshromažďujeme senzor/periférii/vydavatele do sdíleného `IDeviceContainer`. Spojení se budují přesně v kořeni kompozice.
- Nepoužíváme name-based subscriptions ("vydavatel 'telemetry' poslouchá senzor 'sht31'"). Všechna spojení jsou typované ukazatele.

## Související dokumenty

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — vytvoření a pořadí montáže.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — architektonický diagram.
- [04-patterns/01-add-sensor.md](01-add-sensor.md), [02-add-peripheral.md](02-add-peripheral.md), [03-add-transport.md](03-add-transport.md) — konkrétní návody na komponenty.
