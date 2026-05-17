# Modelo de perfil

Um perfil é uma implementação da interface `IProfile` interface, which descreve o comportamento de um dispositivo específico. A biblioteca interage com o produto apenas através de essa interface.

## Interface IProfile

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

### Quando a biblioteca chama cada método

| Method | When called | What it must do |
|--------|------------|----------------|
| `onOnline()` | Na primeira `CloudStateMachine` transição para `Online` | Carregar configuração de NVS, aplicar ao hardware |
| `loop()` | Em cada iteração de `IdryerRuntime::loop()` | Temporizadores, animações, polling de sensor |
| `buildInfoJson(buf, len)` | Na transição to Online; on `ping` | Serializar informações do dispositivo payload |
| `getConfig(out)` | Em `invoke device.getConfig` | Preencher doc com o atual config |
| `applyConfig(id, val)` | Em `comandos/set` | Aplicar parâmetro, guardar em NVS |

## Exemplo: LedStripProfile

`LedStripProfile` é o perfil implementação para Storage Link. Localizado em `src/storage/led_strip/`.

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

`onOnline()` aplica a corrente LED configuração da tira (contagem de LEDs, brilho) a `LedStripExecutor`.

`applyConfig(id, val)` aceita a ID de parâmetro de `menu_ids.h` and um novo valor. Guarda para NVS via the `menu` object. Parameters tal como `strip_type` and `color_order` exigem reinicialização — FastLED é inicializado uma vez em inicialização.

`buildInfoJson` constrói o payload para `idryer/{serial}/info`. Composição de campo é definido por o produto. Storage Link publica:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

Para dispositivos com múltiplas câmaras unidades (iDryer LINK), é típico para adicionar `workTimeCounter`, `unidadesCount`, and a `unidades` array descrevendo capacidades.

## ActionDispatcher

`ActionDispatcher` encaminha dois tipos de comando sem std::function (ponteiros de função simples para economizar heap):

```cpp
// Invoke: action with name and arguments
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: setting a single parameter
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

Registro em `setup()`:

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

`IdryerRuntime` chama `dispatcher.handleInvocar(data)` and `dispatcher.handleDefinir(data)` quando o MQTT correspondente comandos chegam.

## Criar um novo perfil

1. Criar um classe herdando from `IProfile`.
2. Implementar todos cinco métodos.
3. Passar um apontador para o perfil para o `IdryerRuntime` construtor.
4. Registar handlers in `ActionDispatcher` para `invoke` e `set` comandos.

Não há restrições no que o perfil faz dentro seus métodos — it tem total visibility para o product contexto.
