Onboarding é um procedimento único em que o ESP32 se registra na nuvem iDryer e é vinculado à sua conta. Uma vez completo, o dispositivo aparece no portal com estado Online e estado Ready, e todas as inicializações subsequentes são automáticas.


- Um dispositivo ESP32 gravado com uma compilação REPL: env `esp32c3-super-mini-dev` (ver [Comece em 5 minutos](01-five-minutes.md)) ou qualquer uma de suas compilações dev com a bandeira `IDRYER_DEV_REPL=1`.
- Cabo USB.
- Conta em [portal.idryer.org](https://portal.idryer.org/) (para desenvolvimento — [staging.idryer.org](https://staging.idryer.org/)).


O REPL está disponível apenas em compilações com a bandeira `IDRYER_DEV_REPL=1`. Abra Serial Monitor, insira três comandos — o dispositivo se conecta ao WiFi, pede um PIN e está pronto para claim.


```bash
pio run -e esp32c3-super-mini-dev -t upload
```

Ou use qualquer env onde `-DIDRYER_DEV_REPL=1` está definido.


```bash
pio device monitor -b 115200
```

Após a inicialização você verá o prompt:

```
[boot] iDryer dev REPL ready — type 'help'
```

Imediatamente depois, mensagens da stack de nuvem começam a aparecer no log:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```


Insira no console do Serial Monitor:

```
wifi MyHomeWiFi MySecretPass
```

Resposta:

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

As credenciais são escritas em NVS. A placa chama imediatamente `WiFi.begin()`. O log mostrará:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```


O dispositivo provisiona-se automaticamente e registra um PIN de 7 dígitos. O PIN é válido por 10 minutos.

1. Abra [portal.idryer.org](https://portal.idryer.org/) (ou staging).
2. Vá para **Add device**.
3. Insira o PIN do Serial Monitor.

Após um claim bem-sucedido, o log mostra:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Se o PIN expirou antes de você inseri-lo — execute o comando `claim` para obter um novo.


| Comando | O que faz | Quando usar |
|---------|----------|------------|
| `help` | Mostrar lista de comandos | Lembrar a sintaxe |
| `status` | Estado atual: WiFi, IP, RSSI, online, serial | Diagnóstico de conexão |
| `wifi <ssid> <password>` | Salvar credenciais WiFi em NVS e reconectar | Primeiro onboarding ou mudança de rede |
| `claim` | Iniciar manualmente o fluxo de claim, obter novo PIN | PIN expirado ou re-claim necessário |
| `wipe` | Apagar NVS (credenciais, claim, menu) e reiniciar | Reset de fábrica |
| `restart` | Reinício de software do ESP | Reinício rápido sem desconexão física |


Improv-WiFi está integrado em todas as compilações e não depende da bandeira `IDRYER_DEV_REPL`. Adequado para entregar um dispositivo a um usuário ou quando um terminal é inconveniente. Requer Chrome ou Edge — a Web Serial API não é suportada em Safari ou Firefox.


Qualquer compilação prod serve. Improv-WiFi está sempre ativo.


Vá para [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/), clique em **Connect** e selecione a porta USB do dispositivo no diálogo do navegador.


A página pedirá o nome da rede e a senha, transmiti-la-á à placa via Serial-Improv. A placa salva as credenciais em NVS e se conecta ao WiFi. O provisionamento e obtenção de PIN acontecem automaticamente — o mesmo que no Caminho 1.

!!! note
    Improv-WiFi não pode executar `claim`, `wipe` ou verificar `status`. Use o REPL para fluxo de claim manual e gerenciamento de NVS.


| Situação | Recomendação |
|----------|-------------|
| Desenvolvedor embarcado com terminal aberto | REPL |
| Entregar o dispositivo a um usuário | Improv-WiFi |
| Precisa de `wipe` manual ou repeat `claim` | REPL |
| Navegador Safari ou Firefox | REPL |
| PlatformIO não instalado | Improv-WiFi |


**PIN não aparece no log.** Verifique se o dispositivo se conectou ao WiFi: digite `status` e verifique que o campo `ip=` na resposta não está vazio. O provisionamento não começa sem WiFi.

**PIN expirou.** Insira o comando `claim` — o dispositivo pede um novo registro e imprime um PIN fresco.

**Dispositivo já vinculado a outra conta.** Insira `wipe` — NVS será apagado, a placa reiniciará e o onboarding começará do zero.

**PIN não aceito pelo portal.** Verifique se você copiou todos os 7 dígitos sem espaços e se menos de 10 minutos passaram desde o PIN aparecer.

**Improv-WiFi não vê o dispositivo no navegador.** Certifique-se de que você está usando Chrome ou Edge e de que o driver USB do ESP32 está instalado.


- Full Link API: [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Adicione um sensor ou periférico: [../04-patterns/](../04-patterns/)
