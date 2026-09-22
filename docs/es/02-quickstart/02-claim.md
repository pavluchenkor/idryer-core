# Vinculación con la cuenta

La vinculación es un procedimiento único: el dispositivo recibe un token de vinculación de un solo uso, lo cambia en el portal por un secreto permanente y guarda el secreto en NVS. Después se conecta solo al portal tras cada reinicio. Mientras no tiene secreto, el dispositivo está en modo de configuración y espera un token.

## En la aplicación iDryer

1. El dispositivo está en la red (ver [Wi-Fi](01-wifi.md)), el teléfono en la misma red.
2. **Conectar un dispositivo nuevo** → paso **Vinculación** (si el dispositivo ya está en la red, toca el chip del paso en la parte superior de la ventana) → **Vincular**.
3. La aplicación encuentra el dispositivo en la red local, obtiene un token del portal, se lo entrega al dispositivo y espera a que el portal confirme que el dispositivo está en línea.
4. Tras **Dispositivo vinculado**, el dispositivo está en la lista del portal y de la aplicación.

El log antes de la vinculación:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Después:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

El instalador web de firmware pasa el token por USB con el comando `PAIR_TOKEN` (ver abajo).

## Desvinculación

La desvinculación en la aplicación o en el portal llega al dispositivo como el comando `revoke`. Todo firmware necesita el manejador:

```cpp
// Desvinculación en la aplicación o en el portal: borrar el secreto y esperar una nueva vinculación.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` borra el secreto y conserva la red: el dispositivo vuelve a esperar una vinculación. Sin manejador el secreto se queda en el dispositivo, y solo podrá vincularse de nuevo después de `WIPE_IDENTITY`.

## Comandos por USB

Tras unirse a la red, el núcleo acepta líneas por el puerto serie (115200, cada línea termina con un salto de línea):

| Comando | Respuesta | Qué hace |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | estado: `state` es `bound` (vinculado) o `setup` (espera un token), `cloud` es `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | borra el secreto como `revoke`; la red se conserva |
| `PAIR_TOKEN:<token>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | pasa un token de vinculación; un dispositivo vinculado no lo acepta |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Antes de unirse a la red, el puerto está ocupado por Improv y los comandos no responden. En una compilación con `IDRYER_DEV_REPL` el puerto pertenece al producto y estos comandos no existen: ver [Configuración detallada](99-detailed-setup.md).

## Si no funcionó

- la aplicación no encuentra el dispositivo: teléfono y dispositivo en la misma red; las redes de invitados suelen bloquear el descubrimiento de dispositivos;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`, o el dispositivo pertenece a otra cuenta: desvincúlalo en el portal o envía `WIPE_IDENTITY` y vuelve a vincularlo.

## Siguiente

[Telemetría](03-telemetry.md).
