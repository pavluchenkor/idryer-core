# Модель профилей

Профиль — реализация интерфейса `IProfile`, который описывает поведение конкретного устройства. Библиотека взаимодействует с продуктом только через этот интерфейс.

## Интерфейс IProfile

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

### Когда библиотека вызывает каждый метод

| Метод | Когда вызывается | Что должен делать |
|-------|-----------------|-------------------|
| `onOnline()` | При первом переходе `CloudStateMachine` в `Online` | Загрузить конфиг из NVS, применить к железу |
| `loop()` | Каждая итерация `IdryerRuntime::loop()` | Таймеры, анимации, опрос датчиков |
| `buildInfoJson(buf, len)` | При переходе в Online; при получении `ping` | Сериализовать device info payload |
| `getConfig(out)` | При получении `invoke device.getConfig` | Заполнить doc текущим конфигом |
| `applyConfig(id, val)` | При получении `commands/set` | Применить параметр, сохранить в NVS |

## Пример: LedStripProfile

`LedStripProfile` — реализация профиля для Storage Link. Находится в `src/storage/led_strip/`.

```cpp
class LedStripProfile : public IProfile {
public:
    explicit LedStripProfile(LedStripExecutor* executor);

    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;

    static void normalizeGroups();        // исправить NVS-состояние toggle-групп
    static uint8_t selectedStripType();   // 0=WS2812B, 1=APA102
    static uint8_t selectedColorOrder();  // 0=GRB, 1=RGB, 2=BRG, 3=BGR

    static constexpr const char* DEVICE_TYPE = "storage_link";
    static constexpr const char* HW_VERSION  = "1.0";
    static constexpr const char* FW_VERSION  = "1.0.0";

private:
    LedStripExecutor* executor_;
};
```

`onOnline()` применяет текущую конфигурацию LED-ленты (количество светодиодов, яркость) к `LedStripExecutor`.

`applyConfig(id, val)` принимает ID параметра из `menu_ids.h` и новое значение. Сохраняет в NVS через `menu` object. Параметры типа `strip_type` и `color_order` требуют перезагрузки — FastLED инициализируется один раз при старте.

`buildInfoJson` формирует payload для `idryer/{serial}/info`. Состав полей определяется продуктом. Storage Link публикует:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

Для устройств с несколькими chamber-units (iDryer LINK) типично добавлять `workTimeCounter`, `unitsCount` и массив `units` с описанием capabilities.

## ActionDispatcher

`ActionDispatcher` маршрутизирует два типа команд без std::function (plain function pointers для экономии heap):

```cpp
// Invoke: действие с именем и аргументами
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: установка одного параметра
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

Регистрация в `setup()`:

```cpp
// Invoke — делегирует в LedStripExecutor
dispatcher.setInvokeHandler(
    [](const char* action, JsonObjectConst args, void* /*ctx*/) -> bool {
        return s_executor.execute(action, args);
    }, nullptr);

// Set — передаёт id/val в LedStripProfile
dispatcher.setSetCallback(
    [](JsonObjectConst data, void* /*ctx*/) {
        int id  = data["id"]  | -1;
        int val = data["val"] | -1;
        s_profile.applyConfig(id, val);
    }, nullptr);
```

`IdryerRuntime` вызывает `dispatcher.handleInvoke(data)` и `dispatcher.handleSet(data)` при получении соответствующих MQTT-команд.

## Создание нового профиля

1. Создать класс, унаследованный от `IProfile`.
2. Реализовать все пять методов.
3. Передать указатель на профиль в конструктор `IdryerRuntime`.
4. Зарегистрировать обработчики в `ActionDispatcher` для команд `invoke` и `set`.

Ограничений на то, что профиль делает внутри своих методов, нет — он видит весь продуктовый контекст.
