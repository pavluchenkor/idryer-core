# Fluxo de dados entre participantes

Seção aplicada: como sensores, periféricos, perfil, transportes e editores são conectados em código real do produto. A descrição arquitetônica do fluxo de dados está em [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md).

## Princípio

`idryer-core` deliberadamente não fornece um barramento de eventos interno. Todas as conexões entre participantes são **ponteiros explícitos** passados através de construtores na raiz da composição. Isto significa:

- Qualquer fluxo de dados pode ser lido como uma cadeia de ponteiros em `main.cpp`.
- Sem "magia" na descoberta de participantes.
- O produto decide quem passa o quê para quem.

## Mapa de conexão típico para Storage Link

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
        ├──→  ActionDispatcher  ──→  LedStripExecutor (periférico)
        ├──→  IProfile::getConfig  ──→  DevicePublisher::publishConfig
        └──→  IProfile::applyConfig (via onSetCommand)
```

Cada seta é uma linha de passagem de ponteiros em `main.cpp`. Por exemplo:

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     editor
```

## Receita 1 — Sensor publica para a nuvem

**Objetivo**: sensor de temperatura → MQTT.

```
Sensor → Editor → DevicePublisher → MqttClient + LocalAccess
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

`MyTelemetryPublisher::loop` decide quando publicar (por intervalo). Ver [01-add-sensor.md](01-add-sensor.md).

## Receita 2 — Comando de nuvem → periférico

**Objetivo**: `commands/invoke {"action":"led.pulse",...}` → acender LED.

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

Ver [02-add-peripheral.md](02-add-peripheral.md).

## Receita 3 — Comando de app LAN → periférico (mesmo caminho)

**Objetivo**: cliente WS em LAN envia `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` → o mesmo LED acende.

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

Sem novo código necessário — `s_local.setCommandSink(handleCommand)` já mescla ambos os transportes em um manipulador.

## Receita 4 — Sensor → Periférico (loop interno)

**Objetivo**: sensor lê humidade → se acima do limite, ventilador liga.

Esta é lógica interna do produto; `idryer-core` não tem API para tais conexões. Faça diretamente:

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

Conectando na raiz da composição:

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` nada sabe sobre esta classe e não deve.

## Receita 5 — Mudança de configuração → reinicialização de periférico

**Objetivo**: backend envia `commands/set {"id":CFG_BRIGHTNESS,"val":150}` → brilho LED muda imediatamente.

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Periférico
```

```cpp
class MyProfile : public idryer::IProfile {
public:
    MyProfile(MyDevice* a) : device_(a) {}

    bool applyConfig(int id, int val) override {
        if (id == CFG_BRIGHTNESS) {
            menu.brightness = val;
            menu.saveToNVS();
            device_->setBrightness(val);   // aplicar imediatamente
            return true;
        }
        return false;
    }
    // ...
private:
    MyDevice* device_;
};
```

A conexão `perfil → periférico` é construída na raiz da composição:

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## Receita 6 — Novo evento → tópico de eventos

**Objetivo**: periférico captura um erro → evento em `idryer/{serial}/events`.

O periférico não publica por si só. Notifica o produto; o produto publica:

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

// em main.cpp
s_device.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

Alternativamente, o periférico pode aceitar um `DevicePublisher*` através de seu construtor. O ponto chave: a conexão é explícita.

## O que não fazemos

- Não introduzimos um barramento de eventos interno. Isto levaria a conexões ocultas e complexidade de depuração.
- Não coletamos sensor/periférico/editor em um `IDeviceContainer` compartilhado. Conexões são construídas precisamente na raiz da composição.
- Não usamos inscrições baseadas em nome ("editor 'telemetry' escuta sensor 'sht31'"). Todas as conexões são ponteiros tipados.

## Documentos relacionados

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — criação e ordem de montagem.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — diagrama arquitetônico.
- [04-patterns/01-add-sensor.md](01-add-sensor.md), [02-add-peripheral.md](02-add-peripheral.md), [03-add-transport.md](03-add-transport.md) — receitas de componentes concretos.
