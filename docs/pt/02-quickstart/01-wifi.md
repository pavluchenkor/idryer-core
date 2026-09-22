# Wi-Fi

O firmware não tem a palavra-passe da rede. Um dispositivo sem rede guardada espera pela configuração; ao recebê-la, guarda a rede na NVS e daí em diante liga-se sozinho. Só redes de 2,4 GHz.

## A aplicação iDryer (ESPTouch)

O caminho principal: a aplicação envia a rede pelo ar, sem fios.

1. O telemóvel está na rede em que o dispositivo vai funcionar.
2. **Ligar novo dispositivo** → passo **Wi-Fi**: confirme o nome da rede, escreva a palavra-passe, toque em **Ligar dispositivo**.
3. A aplicação envia a configuração durante até 90 segundos e mostra **Dispositivo ligado**.

Com a palavra-passe errada, o dispositivo volta a esperar pela configuração: repita o passo.

## Por USB com Improv

Enquanto o dispositivo não tem rede, o núcleo escuta o protocolo Improv na porta série.

1. Ligue a placa por USB.
2. Abra [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) no Chrome ou no Edge (o Web Serial não funciona no Safari nem no Firefox), clique em **Connect** e escolha a porta da placa.
3. Escreva o nome da rede e a palavra-passe.

Nesse momento a porta está ocupada pelo Improv, por isso o log só aparece quando o dispositivo já está na rede. Feche o monitor série enquanto o Improv trabalha.

## No código, para uma bancada de desenvolvimento

```cpp
void setup() {
    // Só para a bancada de desenvolvimento: a rede é guardada na NVS se ainda lá não estiver.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` só escreve a rede se a NVS ainda não tiver nenhuma; `setWifiCredentials()` reescreve sempre. Chame-as antes de `begin()`.

!!! warning
    Não publique firmware com a palavra-passe no código: quem o descarregar fica com a palavra-passe.

## Verificação

No log:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## A seguir

[Associação à conta](02-claim.md).
