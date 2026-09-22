# Wi-Fi

O firmware não tem a senha da rede. Um dispositivo sem rede salva espera a configuração; ao receber, salva a rede na NVS e dali em diante conecta sozinho. Só redes de 2,4 GHz.

## O app iDryer (ESPTouch)

O caminho principal: o app envia a rede pelo ar, sem fios.

1. O celular está na rede em que o dispositivo vai funcionar.
2. **Conectar novo dispositivo** → passo **Wi-Fi**: confira o nome da rede, digite a senha, toque em **Conectar dispositivo**.
3. O app envia a configuração por até 90 segundos e mostra **Dispositivo conectado**.

Com a senha errada, o dispositivo volta a esperar a configuração: repita o passo.

## Pela USB com Improv

Enquanto o dispositivo não tem rede, o núcleo escuta o protocolo Improv na porta serial.

1. Conecte a placa pela USB.
2. Abra [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) no Chrome ou no Edge (o Web Serial não funciona no Safari nem no Firefox), clique em **Connect** e escolha a porta da placa.
3. Digite o nome da rede e a senha.

Nesse momento a porta está ocupada pelo Improv, por isso o log só aparece quando o dispositivo já está na rede. Feche o monitor serial enquanto o Improv trabalha.

## No código, para uma bancada de desenvolvimento

```cpp
void setup() {
    // Só para a bancada de desenvolvimento: a rede é salva na NVS se ainda não estiver lá.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` só grava a rede se a NVS ainda não tiver nenhuma; `setWifiCredentials()` sobrescreve sempre. Chame antes de `begin()`.

!!! warning
    Não publique firmware com a senha no código: quem baixar fica com a senha.

## Verificação

No log:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## Próximo passo

[Vinculação à conta](02-claim.md).
