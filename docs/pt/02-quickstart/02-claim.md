Depois deste passo o seu dispositivo aparecerá na sua conta [portal.idryer.org](https://portal.idryer.org/) com estado Online. Todos os reinícios subsequentes são automáticos — nova vinculação não é necessária.


O claim é um procedimento único em que o ESP32 se regista na nuvem idryer.org e se vincula à sua conta. O dispositivo gera um PIN de 7 dígitos válido por 10 minutos. Introduz o PIN no portal — vinculação completa.

Após o claim, um `deviceId` é guardado em NVS — o identificador único do dispositivo na nuvem. Nos reinícios subsequentes o ESP32 liga-se directamente a MQTT, sem repetir o fluxo de claim.


- ESP32 gravado do [Passo 01](01-wifi.md) e ligado ao WiFi
- Uma conta em [portal.idryer.org](https://portal.idryer.org/)
- Cabo USB e um Serial Monitor aberto


**1. Verifique se o sketch contém auto-claim.** A linha seguinte deve estar em `setup()` (já está presente no exemplo `03_with_improv`):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Esta callback executa-se automaticamente quando o dispositivo chega à internet e detecta que ainda não está vinculado.

**2. Abra o Serial Monitor** e reinicie a placa:

```bash
pio device monitor -b 115200
```

**3. Aguarde o PIN no registo.** Após WiFi → provisioning → awaiting claim:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

O dispositivo está à espera. O PIN é válido por 10 minutos.

**4. Vá para [portal.idryer.org](https://portal.idryer.org/)** e navegue para **Adicionar dispositivo**.

**5. Introduza o PIN** do Serial Monitor (7 dígitos, sem espaços).

**6. Confirme a vinculação** no portal. O Serial Monitor mostrará então:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```


Abra a lista de dispositivos no portal — o dispositivo deve aparecer com estado **Online**. O LED incorporado começará a piscar uma vez a cada 500 ms (se está a usar o exemplo `01_blink_status`).

!!! note
    Se o PIN expirou (mais de 10 minutos passaram) — reinicie a placa. Auto-claim gerará um novo PIN.

!!! warning
    Se o dispositivo já está vinculado a outra conta, introduza o comando `wipe` no Serial Monitor com `IDRYER_DEV_REPL=1` activado. NVS será apagado, a placa reiniciará e o claim começará do zero.


- [03-telemetry.md](03-telemetry.md) — ligue um sensor e publique leituras no portal.
- [02-onboarding.md](02-onboarding.md) — documentação detalhada de onboarding para caminhos REPL e Improv.
