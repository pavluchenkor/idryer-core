# Profil-Modell

Ein Profil ist eine Implementierung der `IProfile` Schnittstelle, die das Verhalten eines spezifischen Geräts beschreibt. Die Bibliothek interagiert mit dem Produkt nur über diese Schnittstelle.

## IProfile-Schnittstelle

```cpp
class IProfile {
public:
    virtual ~IProfile() = default;

    virtual void onOnline() = 0;
    virtual void loop() = 0;
    virtual void getConfig(JsonDocument& out) = 0;
    virtual bool applyConfig(int id, int val) = 0;
    virtual void buildInfoJson(char* buf, size_t len) const = 0;
};
```

### Wann die Bibliothek jede Methode aufruft

| Methode | Wann aufgerufen | Was es tun muss |
|--------|------------|----------------|
| `onOnline()` | Beim ersten `CloudStateMachine` Übergang zu `Online` | Konfiguration aus NVS laden, auf Hardware anwenden |
| `loop()` | Bei jeder Iteration von `IdryerRuntime::loop()` | Timer, Animationen, Sensor-Polling |
| `buildInfoJson(buf, len)` | Beim Übergang zu Online; auf `ping` | Geräteinformationen-Payload serialisieren |
| `getConfig(out)` | Auf `invoke device.getConfig` | Dokument mit aktueller Konfiguration füllen |
| `applyConfig(id, val)` | Auf `commands/set` | Parameter anwenden, in NVS speichern |

## Beispiel: LedStripProfile

`LedStripProfile` ist die Profil-Implementierung für Storage Link. Befindet sich in `src/storage/led_strip/`.

```cpp
class LedStripProfile : public IProfile {
public:
    explicit LedStripProfile(LedStripExecutor* executor);

    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;

    static void normalizeGroups();        // fix NVS state of toggle groups
    static uint8_t selectedStripType();   // 0=WS2812B, 1=APA102
    static uint8_t selectedColorOrder();  // 0=GRB, 1=RGB, 2=BRG, 3=BGR

    static constexpr const char* DEVICE_TYPE = "storage_link";
    static constexpr const char* HW_VERSION  = "1.0";
    static constexpr const char* FW_VERSION  = "1.0.0";

private:
    LedStripExecutor* executor_;
};
```

`onOnline()` wendet die aktuelle LED-Streifen-Konfiguration (LED-Anzahl, Helligkeit) auf `LedStripExecutor` an.

`applyConfig(id, val)` akzeptiert eine Parameter-ID aus `menu_ids.h` und einen neuen Wert. Speichert über das `menu` Objekt in NVS. Parameter wie `strip_type` und `color_order` benötigen einen Neustart — FastLED wird einmal beim Start initialisiert.

`buildInfoJson` erstellt die Payload für `idryer/{serial}/info`. Die Feldkomposition wird vom Produkt definiert. Storage Link veröffentlicht:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

Für Geräte mit mehreren Kammer-Einheiten (iDryer LINK) ist es typisch, `workTimeCounter`, `unitsCount` und ein `units` Array, das Fähigkeiten beschreibt, hinzuzufügen.

## ActionDispatcher

`ActionDispatcher` leitet zwei Befehlstypen ohne std::function (schlichte Funktionszeiger zur Heapeinsparung):

```cpp
// Invoke: action with name and arguments
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: setting a single parameter
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

Registrierung in `setup()`:

```cpp
// Invoke — delegates to LedStripExecutor
dispatcher.setInvokeHandler(
    [](const char* action, JsonObjectConst args, void* /*ctx*/) -> bool {
        return s_executor.execute(action, args);
    }, nullptr);

// Set — passes id/val to LedStripProfile
dispatcher.setSetCallback(
    [](JsonObjectConst data, void* /*ctx*/) {
        int id  = data["id"]  | -1;
        int val = data["val"] | -1;
        s_profile.applyConfig(id, val);
    }, nullptr);
```

`IdryerRuntime` ruft `dispatcher.handleInvoke(data)` und `dispatcher.handleSet(data)` auf, wenn die entsprechenden MQTT-Befehle eintreffen.

## Ein neues Profil erstellen

1. Erstellen Sie eine Klasse, die von `IProfile` erbt.
2. Implementieren Sie alle fünf Methoden.
3. Übergeben Sie einen Zeiger auf das Profil an den `IdryerRuntime` Konstruktor.
4. Registrieren Sie Handler in `ActionDispatcher` für `invoke` und `set` Befehle.

Es gibt keine Einschränkungen, was das Profil in seinen Methoden tut — es hat vollständige Sichtbarkeit in den Produkt-Kontext.
