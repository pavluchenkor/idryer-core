# Ligação Wi-Fi e modo de configuração

O dispositivo recebe a palavra-passe da rede por dois caminhos: por cabo, a partir do instalador web (protocolo Improv), e pelo ar, a partir da aplicação móvel **iDryer** para Android e iOS (protocolo ESPTouch v2). Depois disso a palavra-passe fica na NVS e sobrevive a reinícios.

Disto trata o `EspTouchProvisioner` (`platform/arduino/`). O produto não precisa de chamar nada — o `Link` arranca-o e conduz-no; os callbacks servem apenas para mostrar o progresso da configuração num ecrã.

Os protocolos não são compatíveis entre versões: a aplicação tem de enviar ESPTouch **v2**. Na primeira versão os dados eram transportados pelos comprimentos das tramas, e estes partiam-se sempre que um router retransmitia o broadcast entre as bandas de 2,4 e 5 GHz — uma única rede mesh com nome partilhado funcionava apenas de vez em quando.

Vale a pena perceber este comportamento por uma razão: o dispositivo deve restabelecer a ligação em silêncio quando consegue, e chamar o dono apenas quando não consegue. É fácil errar nos dois sentidos — ou incomodar o dono a cada soluço do router, ou ficar calado offline com uma palavra-passe errada.

## Quatro fases

O provisionador vive **apenas até à primeira ligação bem-sucedida**. O `Link::loop()` chama-o enquanto não há Wi-Fi; depois do primeiro `WL_CONNECTED` a máquina de estados da cloud assume, e o modo de configuração nunca mais arranca — até ao reinício seguinte.

### Fase 1. Arranque, a palavra-passe está em memória

Modo persistente, sem diagnóstico.

A cada `kBootRetryMs` (6 segundos) uma nova tentativa de ligação. Isto prossegue durante `kConnectFallbackMs` (90 segundos). Ligou — trabalhamos; não ligou — arranca o modo de configuração.

Porquê sem diagnóstico: a palavra-passe não é nova, em algum momento funcionou. Não é preciso apurar se o sinal está fraco ou se a palavra-passe mudou — se não houver ligação dentro da janela, chamamos o dono e ele resolve no local.

Porque são precisas as repetições: o `WiFi.begin()` é chamado uma única vez, a partir do `Link::begin()`. A máquina de estados da cloud não corre antes do `WL_CONNECTED`, e a reconexão automática do Arduino não cobre os `reason` 202 e 205. Sem repetições há exactamente uma tentativa e, com sinal fraco, o dispositivo entra em configuração com uma palavra-passe perfeitamente válida na memória.

Porquê 90 segundos — a contar do pior caso, quando a energia regressa e tudo arranca ao mesmo tempo:

- **até 60 segundos** — o arranque do router. Requisito da indústria do Broadband Forum, [TR-124 Issue 9](https://rg-device-requirements.broadband-forum.org/), ponto `GEN.OPS.9`: «The RG MUST complete power up in 60 seconds or less». Ressalva: o requisito cobre o arranque completo da gateway, não existe ali um limite próprio para o aparecimento do SSID. As medições em equipamento de grande consumo dão 34–49 segundos até estar no ar, pelo que 60 é um limite superior razoável e não uma média;
- **mais a nossa própria ligação** — habitualmente 2–5 segundos, mas com um `RSSI` na ordem dos −85 dBm estica-se até vinte.

Daí os 90: sessenta para o router e trinta para nós. Menos arrisca chamar o dono mesmo antes de a rede aparecer; bastante mais deixa-o demasiado tempo a olhar para um aparelho sem ligação e sem explicação.

Um router e um ponto de acesso ligados em conjunto arrancam em paralelo, não um a seguir ao outro — os tempos não se somam. Se usarmos o critério mais exigente — «o cliente obteve endereço e chega à internet» — a conta sobe para 90–120 segundos: aí entram ainda a WAN, o DHCP ou o PPPoE e a sincronização do modem. Não precisamos de esperar por isso — ao dispositivo basta entrar na rede local.

### Fase 2. Arranque, sem palavra-passe em memória

O modo de configuração arranca de imediato — não há nada por que esperar.

### Fase 3. A palavra-passe acabou de chegar

Modo inteligente, `driveConnect()`.

Aqui a palavra-passe é **recente e pode estar errada** — o dono pode ter-se enganado a escrever. Por isso o provisionador distingue as causas da falha:

- conta as falhas de autenticação consecutivas (`kAuthFailLimit`, 8);
- depois da quinta observa o ar e guarda o melhor `RSSI`;
- se o `RSSI` for pior que `kWeakRssi` (−80 dBm), o regresso é cancelado: os mesmos `reason` 15 e 202 também surgem da perda de tramas de autenticação com a palavra-passe correcta, e a ligação pode levar minutos. Ali vale outro prazo — `kWeakSignalGiveUpMs` (10 minutos);
- com sinal firme e oito falhas regressa ao modo de configuração: caso contrário, de uma gralha não há saída sem reprogramar.

Além disso, um watchdog para o silêncio total (`kWatchdogMs`, 20 segundos): quando não há nem eventos nem ligação, a pilha é acordada com `esp_wifi_disconnect()`.

### Fase 4. A ligação cai durante o funcionamento

O provisionador já não é chamado. A ligação é reposta em silêncio pela máquina de estados da cloud, o modo de configuração não arranca — o dono não é incomodado. Do ponto de vista do dispositivo, «desligaram o router à noite» e «trocaram o router» são indistinguíveis, e ele nem tenta adivinhar.

## Como dizer ao dispositivo que a rede mudou

Reiniciá-lo.

Depois do reinício começa a fase 1: a palavra-passe antiga não serve, passam 90 segundos em vão, arranca o modo de configuração — e pode enviar-se uma nova palavra-passe pelo telemóvel ou ligar o cabo. Sem heurísticas, apenas desligar e voltar a ligar.

## Uma propriedade: a partir do modo de configuração a rede não é reapanhada sozinha

Enquanto o dispositivo escuta o ar, **não tenta ligar-se** à rede guardada: o rádio está ocupado a receber, e as tentativas de ligação ficam suspensas até chegar uma nova palavra-passe.

Consequência prática: se o router demorou mais de 90 segundos a arrancar e o dispositivo já entrou em configuração, não regressa sozinho à rede, mesmo depois de esta aparecer. Ou se envia a palavra-passe, ou se reinicia o dispositivo.

É uma propriedade do modo, não um descuido: escutar o ar e estabelecer uma ligação ao mesmo tempo não é possível.

## Tempos-limite

| Constante | Valor | O que define |
|---|---|---|
| `kBootRetryMs` | 6 s | intervalo entre tentativas na fase 1 |
| `kConnectFallbackMs` | 90 s | quanto tempo tentamos antes de chamar o dono |
| `kReconnectDelayMs` | 700 ms | pausa após uma quebra antes de nova tentativa (fase 3) |
| `kWatchdogMs` | 20 s | silêncio da pilha ao fim do qual esta é acordada |
| `kAuthFailLimit` | 8 | falhas de autenticação seguidas até regressar à configuração |
| `kWeakRssi` | −80 dBm | limite de «sinal fraco» |
| `kWeakSignalGiveUpMs` | 10 min | quanto toleramos um sinal fraco antes de desistir |
| `kRestartMs` | 10 min | reinício do modo de configuração se a palavra-passe nunca chegar |

## O que o dono vê

A entrada no modo de configuração dispara o callback `Notice`:

- `Listening` — configuração normal: ou não há rede em memória, ou a ligação já funcionou antes;
- `CheckPassword` — a rede está definida mas nunca deixou o dispositivo entrar. A única explicação que resta é palavra-passe errada.

Como apresentar isto decide o produto. No `idryer-touch` é um aviso em ecrã inteiro.
