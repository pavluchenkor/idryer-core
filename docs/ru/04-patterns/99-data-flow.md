# Поток данных между участниками

Прикладной раздел: как в реальном продуктовом коде sensors, peripherals, profile, transports и publishers связываются между собой. Архитектурное описание потоков — в [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md).

## Принцип

`idryer-core` сознательно не предоставляет внутренний event bus. Все связи между участниками — **явные указатели**, переданные через конструкторы в composition root. Это значит:

- Любой поток данных читается как цепочка указателей в `main.cpp`.
- Никакого "магического" обнаружения участников.
- Продукт сам решает, кто кому что передаёт.

## Карта типовых связей в Storage Link

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
        ├──→  ActionDispatcher  ──→  LedStripExecutor (периферия)
        ├──→  IProfile::getConfig  ──→  DevicePublisher::publishConfig
        └──→  IProfile::applyConfig (через onSetCommand)
```

Каждая стрелка — одна строка передачи указателя в `main.cpp`. Например:

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     publisher
```

## Рецепт 1 — Sensor публикует в облако

**Цель**: датчик температуры → MQTT.

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

`MyTelemetryPublisher::loop` сам решает, когда публиковать (по интервалу). См. [01-add-sensor.md](01-add-sensor.md).

## Рецепт 2 — Команда из облака → периферия

**Цель**: `commands/invoke {"action":"led.pulse",...}` → включить LED.

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

См. [02-add-peripheral.md](02-add-peripheral.md).

## Рецепт 3 — Команда из приложения по LAN → периферия (тот же путь)

**Цель**: WS-клиент в LAN отправляет `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → тот же LED включается.

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

Никакого нового кода — `s_local.setCommandSink(handleCommand)` уже сводит оба transport'а в один обработчик.

## Рецепт 4 — Датчик → Периферия (внутреннее замыкание)

**Цель**: датчик читает влажность → если выше порога, включается вентилятор.

Это внутрипродуктовая логика, у `idryer-core` нет API для таких связей. Делайте напрямую:

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

Connecting в composition root:

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` об этом классе ничего не знает и не должен знать.

## Рецепт 5 — Изменение конфига → переинициализация периферии

**Цель**: backend прислал `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → яркость LED меняется немедленно.

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Периферия
```

```cpp
class MyProfile : public idryer::IProfile {
public:
    MyProfile(MyDevice* a) : device_(a) {}

    bool applyConfig(int id, int val) override {
        if (id == CFG_BRIGHTNESS) {
            menu.brightness = val;
            menu.saveToNVS();
            device_->setBrightness(val);   // мгновенное применение
            return true;
        }
        return false;
    }
    // ...
private:
    MyDevice* device_;
};
```

Связь `profile → периферия` строится в composition root:

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## Рецепт 6 — Новое событие → events топик

**Цель**: периферия поймала ошибку → событие в `idryer/{serial}/events`.

Периферия не публикует сама. Он сообщает продукту, продукт публикует:

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

// в main.cpp
s_device.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

Можно и проще — периферия принимает `DevicePublisher*` через конструктор. Главное: связь явная.

## Рецепт 7 — Внешняя интеграция → периферия

**Цель**: целевая температура камеры от стороннего принтера (Klipper или Bambu) → импульс на нагреватель.

В отличие от рецептов 1–3, входящий поток сюда приходит **не от backend через MQTT-команды**, а от чужой системы через её собственный протокол (Klipper WebSocket, Bambu MQTT). Менеджер интеграций (`LinkIntegrationsManager`) держит соединение, парсит сообщения и вызывает зарегистрированный колбэк.

```
Принтер → intManager.loop() → callback (продукт) → периферия
```

Реализация в продукте — модуль-«мост», знающий про конкретное железо:

```cpp
// heater/auto_heat.h
namespace iheaterlink {
    void wireAutoHeat(RmtOutputAdapter* output);
    void onVirtualChamberUpdate(const idryer::cloud::VirtualChamberData& data);
    void onBambuPrinterStatusUpdate(const idryer::cloud::BambuPrinterStatus& status);
}
```

```cpp
// heater/auto_heat.cpp
namespace iheaterlink {
namespace { RmtOutputAdapter* g_output = nullptr; }

void wireAutoHeat(RmtOutputAdapter* output) { g_output = output; }

void onVirtualChamberUpdate(const idryer::cloud::VirtualChamberData& data) {
    if (!g_output) return;
    ControllerOutputCommand cmd{};
    if (data.available && data.target > 0.0f) {
        cmd.mode = ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = data.target;
    } else {
        cmd.mode = ControllerOutputMode::Off;
    }
    g_output->apply(cmd);
}
} // namespace iheaterlink
```

Composition root связывает три участника одной строкой:

```cpp
static iheaterlink::RmtOutputAdapter s_output{...};

void setup() {
    intManager.begin();

    iheaterlink::wireAutoHeat(&s_output);
    intManager.setVirtualChamberCallback(iheaterlink::onVirtualChamberUpdate);
    intManager.setBambuPrinterStatusCallback(iheaterlink::onBambuPrinterStatusUpdate);
}

void loop() {
    intManager.loop();   // здесь произойдёт парсинг и вызов колбэков
}
```

Жизненный цикл колбэков (когда они вызываются, что произойдёт если не подписать) — см. [07-advanced/03-integrations.md](../07-advanced/03-integrations.md#жизненный-цикл-колбэков).

Почему `auto_heat` живёт в продукте, а не в `idryer-core`:

- `RmtOutputAdapter` — продуктовое железо. У других устройств (iDryer Dryer, Storage Link) его нет.
- Политика «target>0 → греть» тоже продуктовая. У iHeater Link она такая, у другого устройства может быть гистерезис, ограничения, блокировки по другим датчикам.
- SDK даёт «сырое» событие от принтера; решение, что с ним делать, остаётся продукту.

## Чего не делаем

- Не вводим внутренний event bus. Это привело бы к скрытым связям и сложности отладки.
- Не складываем sensor/peripheral/publisher в общий `IDeviceContainer`. Связи строятся точечно в composition root.
- Не делаем подписку через имена ("publisher 'telemetry' слушает sensor 'sht31'"). Все связи — типизированные указатели.

## Связанные документы

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — порядок создания и сборки.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — архитектурная схема.
- [04-patterns/01-add-sensor.md](01-add-sensor.md), [02-add-peripheral.md](02-add-peripheral.md), [03-add-transport.md](03-add-transport.md) — конкретные рецепты компонентов.
