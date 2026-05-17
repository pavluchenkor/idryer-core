# Model profilu

Profil je implementace rozhraní `IProfile`, které popisuje chování konkrétního zařízení. Knihovna interaguje s produktem pouze prostřednictvím tohoto rozhraní.

## Rozhraní IProfile

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

### Kdy knihovna volá jednotlivé metody

| Metoda | Kdy se volá | Co musí udělat |
|--------|------------|----------------|
| `onOnline()` | Při prvním přechodu `CloudStateMachine` do `Online` | Načíst konfiguraci z NVS, použít na hardware |
| `loop()` | Při každé iteraci `IdryerRuntime::loop()` | Časovače, animace, dotazování senzorů |
| `buildInfoJson(buf, len)` | Při přechodu do Online; při `ping` | Serializovat datovou část info zařízení |
| `getConfig(out)` | Při `invoke device.getConfig` | Vyplnit dokument aktuální konfigurací |
| `applyConfig(id, val)` | Při `commands/set` | Použít parametr, uložit do NVS |

## Příklad: LedStripProfile

`LedStripProfile` je implementace profilu pro Storage Link. Nachází se v `src/storage/led_strip/`.

```cpp
class LedStripProfile : public IProfile {
public:
    explicit LedStripProfile(LedStripExecutor* executor);

    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;

    static void normalizeGroups();        // opravit stav NVS skupin přepínačů
    static uint8_t selectedStripType();   // 0=WS2812B, 1=APA102
    static uint8_t selectedColorOrder();  // 0=GRB, 1=RGB, 2=BRG, 3=BGR

    static constexpr const char* DEVICE_TYPE = "storage_link";
    static constexpr const char* HW_VERSION  = "1.0";
    static constexpr const char* FW_VERSION  = "1.0.0";

private:
    LedStripExecutor* executor_;
};
```

`onOnline()` aplikuje aktuální konfiguraci LED pásu (počet LED diod, jas) na `LedStripExecutor`.

`applyConfig(id, val)` přijímá ID parametru z `menu_ids.h` a novou hodnotu. Uloží do NVS prostřednictvím objektu `menu`. Parametry jako `strip_type` a `color_order` vyžadují restart — FastLED je inicializován jednou při spuštění.

`buildInfoJson` vytváří datovou část pro `idryer/{serial}/info`. Složení pole je definováno produktem. Storage Link publikuje:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

Pro zařízení s více jednotkami komory (iDryer LINK) je typické přidat `workTimeCounter`, `unitsCount` a pole `units` popisující schopnosti.

## ActionDispatcher

`ActionDispatcher` směruje dva typy příkazů bez std::function (prosté ukazatele funkcí pro úsporu haldy):

```cpp
// Invoke: akce s názvem a argumenty
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: nastavení jednoho parametru
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

Registrace v `setup()`:

```cpp
// Invoke — delegáty na LedStripExecutor
dispatcher.setInvokeHandler(
    [](const char* action, JsonObjectConst args, void* /*ctx*/) -> bool {
        return s_executor.execute(action, args);
    }, nullptr);

// Set — předej id/val na LedStripProfile
dispatcher.setSetCallback(
    [](JsonObjectConst data, void* /*ctx*/) {
        int id  = data["id"]  | -1;
        int val = data["val"] | -1;
        s_profile.applyConfig(id, val);
    }, nullptr);
```

`IdryerRuntime` volá `dispatcher.handleInvoke(data)` a `dispatcher.handleSet(data)` při příjmu odpovídajících MQTT příkazů.

## Vytvoření nového profilu

1. Vytvořte třídu dědící z `IProfile`.
2. Implementujte všech pět metod.
3. Předejte ukazatel na profil do konstruktoru `IdryerRuntime`.
4. Zaregistrujte obslužné programy v `ActionDispatcher` pro příkazy `invoke` a `set`.

Neexistují žádná omezení, co profil dělá uvnitř svých metod — má úplnou viditelnost do kontextu produktu.
