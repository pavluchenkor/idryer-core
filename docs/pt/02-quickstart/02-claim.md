# Associação à conta

A associação é um procedimento único: o dispositivo recebe um token de associação de uso único, troca-o no portal por um segredo permanente e guarda o segredo na NVS. Depois liga-se sozinho ao portal após cada reinício. Enquanto não tem segredo, o dispositivo está em modo de configuração e espera por um token.

## Na aplicação iDryer

1. O dispositivo está na rede (ver [Wi-Fi](01-wifi.md)), o telemóvel na mesma rede.
2. **Ligar novo dispositivo** → passo **Vinculação** (se o dispositivo já estiver na rede, toque no chip do passo no topo da janela) → **Emparelhar**.
3. A aplicação encontra o dispositivo na rede local, obtém um token do portal, entrega-o ao dispositivo e espera que o portal confirme que o dispositivo está online.
4. Depois de **Dispositivo emparelhado**, o dispositivo está na lista do portal e da aplicação.

O log antes da associação:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Depois:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

O instalador web de firmware passa o token por USB com o comando `PAIR_TOKEN` (ver abaixo).

## Desassociação

A desassociação na aplicação ou no portal chega ao dispositivo como o comando `revoke`. Todo o firmware precisa do handler:

```cpp
// Desassociação na aplicação ou no portal: apagar o segredo e esperar por nova associação.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` apaga o segredo e mantém a rede: o dispositivo volta a esperar pela associação. Sem handler, o segredo fica no dispositivo, e só se consegue associá-lo de novo depois de `WIPE_IDENTITY`.

## Comandos por USB

Depois de entrar na rede, o núcleo aceita linhas na porta série (115200, cada linha termina com uma quebra de linha):

| Comando | Resposta | O que faz |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | estado: `state` é `bound` (associado) ou `setup` (espera um token), `cloud` é `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | apaga o segredo como `revoke`; a rede mantém-se |
| `PAIR_TOKEN:<token>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | passa um token de associação; um dispositivo associado não o aceita |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Antes de entrar na rede, a porta está ocupada pelo Improv e os comandos não respondem. Numa compilação com `IDRYER_DEV_REPL`, a porta pertence ao produto e estes comandos não existem: ver [Configuração detalhada](99-detailed-setup.md).

## Se não funcionou

- a aplicação não encontra o dispositivo: telemóvel e dispositivo na mesma rede; as redes de convidados bloqueiam muitas vezes a descoberta de dispositivos;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`, ou o dispositivo pertence a outra conta: desassocie-o no portal ou envie `WIPE_IDENTITY` e associe-o de novo.

## A seguir

[Telemetria](03-telemetry.md).
