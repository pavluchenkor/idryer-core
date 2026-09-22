# Vinculação à conta

A vinculação é um procedimento único: o dispositivo recebe um token de vinculação de uso único, troca no portal por um segredo permanente e salva o segredo na NVS. Depois ele conecta sozinho ao portal a cada reinício. Enquanto não tem segredo, o dispositivo fica em modo de configuração e espera um token.

## No app iDryer

1. O dispositivo está na rede (veja [Wi-Fi](01-wifi.md)), o celular na mesma rede.
2. **Conectar novo dispositivo** → passo **Vinculação** (se o dispositivo já estiver na rede, toque no chip do passo no topo da janela) → **Vincular**.
3. O app encontra o dispositivo na rede local, pega um token no portal, entrega ao dispositivo e espera o portal confirmar que o dispositivo está online.
4. Depois de **Dispositivo vinculado**, o dispositivo está na lista do portal e do app.

O log antes da vinculação:

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

O instalador web de firmware passa o token pela USB com o comando `PAIR_TOKEN` (veja abaixo).

## Desvinculação

A desvinculação no app ou no portal chega ao dispositivo como o comando `revoke`. Todo firmware precisa do handler:

```cpp
// Desvinculação no app ou no portal: apagar o segredo e esperar uma nova vinculação.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` apaga o segredo e mantém a rede: o dispositivo volta a esperar a vinculação. Sem o handler, o segredo fica no dispositivo, e só dá para vincular de novo depois de `WIPE_IDENTITY`.

## Comandos pela USB

Depois de entrar na rede, o núcleo aceita linhas na porta serial (115200, cada linha termina com uma quebra de linha):

| Comando | Resposta | O que faz |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | estado: `state` é `bound` (vinculado) ou `setup` (espera um token), `cloud` é `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | apaga o segredo como `revoke`; a rede continua |
| `PAIR_TOKEN:<token>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | passa um token de vinculação; um dispositivo vinculado não aceita |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Antes de entrar na rede, a porta está ocupada pelo Improv e os comandos não respondem. Num build com `IDRYER_DEV_REPL`, a porta é do produto e esses comandos não existem: veja [Configuração detalhada](99-detailed-setup.md).

## Se não deu certo

- o app não acha o dispositivo: celular e dispositivo na mesma rede; redes de convidados costumam bloquear a descoberta de dispositivos;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`, ou o dispositivo é de outra conta: desvincule no portal ou envie `WIPE_IDENTITY` e vincule de novo.

## Próximo passo

[Telemetria](03-telemetry.md).
