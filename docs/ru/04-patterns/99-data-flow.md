# Поток данных между участниками

Прикладной раздел: как в реальном продуктовом коде sensors, actuators, profile, transports и publishers связываются между собой. Архитектурное описание потоков — в [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md).

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
        ├──→  ActionDispatcher  ──→  LedStripExecutor (actuator)
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

## Рецепт 2 — Команда из облака → актуатор

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

См. [02-add-actuator.md](02-add-actuator.md).

## Рецепт 3 — Команда из приложения по LAN → актуатор (тот же путь)

**Цель**: WS-клиент в LAN отправляет `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → тот же LED включается.

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

Никакого нового кода — `s_local.setCommandSink(handleCommand)` уже сводит оба transport'а в один обработчик.

## Рецепт 4 — Sensor → Actuator (внутреннее замыкание)

**Цель**: датчик читает влажность → если выше порога, включается вентилятор.

Это внутрипродуктовая логика, у `idryer-core` нет API для таких связей. Делайте напрямую:

```cpp
class HumidityController {
public:
    HumidityController(IClimateSensor* sensor, FanActuator* fan, float threshold)
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
    FanActuator*    fan_;
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

## Рецепт 5 — Изменение конфига → переинициализация actuator

**Цель**: backend прислал `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → яркость LED меняется немедленно.

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Actuator
```

```cpp
class MyProfile : public idryer::IProfile {
public:
    MyProfile(MyActuator* a) : actuator_(a) {}

    bool applyConfig(int id, int val) override {
        if (id == CFG_BRIGHTNESS) {
            menu.brightness = val;
            menu.saveToNVS();
            actuator_->setBrightness(val);   // мгновенное применение
            return true;
        }
        return false;
    }
    // ...
private:
    MyActuator* actuator_;
};
```

Связь `profile → actuator` строится в composition root:

```cpp
static MyActuator s_actuator;
static MyProfile  s_profile(&s_actuator);
```

## Рецепт 6 — Новое событие → events топик

**Цель**: actuator поймал ошибку → событие в `idryer/{serial}/events`.

Actuator не публикует сам. Он сообщает продукту, продукт публикует:

```cpp
class MyActuator {
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
s_actuator.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

Можно и проще — actuator принимает `DevicePublisher*` через конструктор. Главное: связь явная.

## Чего не делаем

- Не вводим внутренний event bus. Это привело бы к скрытым связям и сложности отладки.
- Не складываем sensor/actuator/publisher в общий `IDeviceContainer`. Связи строятся точечно в composition root.
- Не делаем подписку через имена ("publisher 'telemetry' слушает sensor 'sht31'"). Все связи — типизированные указатели.

## Связанные документы

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — порядок создания и сборки.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — архитектурная схема.
- [04-patterns/01-add-sensor.md](01-add-sensor.md), [02-add-actuator.md](02-add-actuator.md), [03-add-transport.md](03-add-transport.md) — конкретные рецепты компонентов.
