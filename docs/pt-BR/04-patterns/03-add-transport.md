# Publicação via devicePublisher

## Quando usar

`iDryer::Link` já contém dois transportes integrados: MQTT (nuvem) e Local WebSocket (LAN). Um transporte adicional não é necessário para a maioria das tarefas.

Use `s_link.devicePublisher()` quando o produto monta sua própria carga útil e deve enviá-la para ambos os canais simultaneamente — por exemplo, quando publicar a configuração do menu em resposta a `commands/get_config`.

## Código pronto para usar

```cpp
// main.cpp (fragmento)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// Publique uma carga útil JSON arbitrária para MQTT e Local WS em uma única chamada.
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // função do produto
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

Uma única chamada `publishConfigRaw` entrega a carga útil para o tópico MQTT `idryer/{serial}/config` e para todos os clientes LAN WS ativos. Não há necessidade de criar clientes ou tópicos adicionais.

## Explicação

`devicePublisher()` é o auxiliar de publicação dupla da fachada. Use em vez de chamar `mqttClient()` ou `LocalAccess` diretamente, a menos que você precise publicar em um tópico não-padrão.

Telemetria e status são publicados automaticamente pela fachada em um temporizador — `devicePublisher()` não é necessário para aqueles.

## Quando um terceiro transporte é necessário

Adicionar um terceiro canal (BLE, Serial JSON, UART proxy) é uma extensão arquitetônica da fachada, não um padrão de receita. A grande maioria dos dispositivos não requer isso.

Se você realmente precisar — os pontos de entrada estão em `lib/idryer-core/src/cloud/` (máquina de estados da nuvem, MQTT) e `lib/idryer-core/src/` (acesso local). Antes de prosseguir, confirme que o MQTT e Local WS integrados são insuficientes para seu caso de uso.

## Exemplo completo no repositório

`publishFullMenu()` em `iDryer-Storage/src/main.cpp:171` — publicação do menu JSON completo via `s_link.devicePublisher()->publishConfigRaw(buf, len)`.
