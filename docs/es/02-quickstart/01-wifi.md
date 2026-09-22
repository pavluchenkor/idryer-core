# Wi-Fi

El firmware no tiene la contraseña de la red. Un dispositivo sin red guardada espera la configuración; al recibirla, guarda la red en NVS y desde entonces se conecta solo. Solo redes de 2,4 GHz.

## La aplicación iDryer (ESPTouch)

La vía principal: la aplicación envía la red por aire, sin cables.

1. El teléfono está en la red en la que funcionará el dispositivo.
2. **Conectar un dispositivo nuevo** → paso **Wi-Fi**: comprueba el nombre de la red, escribe la contraseña, toca **Conectar dispositivo**.
3. La aplicación envía la configuración durante hasta 90 segundos y muestra **Dispositivo conectado**.

Con una contraseña errónea el dispositivo vuelve a esperar la configuración: repite el paso.

## Por USB con Improv

Mientras el dispositivo no tiene red, el núcleo escucha el protocolo Improv en el puerto serie.

1. Conecta la placa por USB.
2. Abre [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) en Chrome o Edge (Web Serial no funciona en Safari ni en Firefox), pulsa **Connect** y elige el puerto de la placa.
3. Escribe el nombre de la red y la contraseña.

En ese momento el puerto está ocupado por Improv, así que el log aparece solo cuando el dispositivo ya está en la red. Cierra el monitor serie mientras trabaja Improv.

## En el código, para un banco de desarrollo

```cpp
void setup() {
    // Solo para el banco de desarrollo: la red se guarda en NVS si aún no está allí.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` escribe la red solo si NVS aún no tiene ninguna; `setWifiCredentials()` sobrescribe siempre. Llámalas antes de `begin()`.

!!! warning
    No publiques firmware con la contraseña en el código: la obtiene cualquiera que lo descargue.

## Comprobación

En el log:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## Siguiente

[Vinculación con la cuenta](02-claim.md).
