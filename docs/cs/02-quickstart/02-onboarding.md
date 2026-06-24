Onboarding je jednorázový postup, ve kterém se ESP32 zaregistruje v cloudu iDryer a je připojen k vašemu účtu. Jakmile je hotovo, zařízení se zobrazí na portálu se stavem Online a stavem Ready, a všechny následné spuštění jsou automatické.


- Zařízení ESP32 naflashované s REPL buildem: env `esp32c3-super-mini-dev` (viz [Začnete v 5 minutách](01-five-minutes.md)) nebo jakýkoliv váš dev build s příznakem `IDRYER_DEV_REPL=1`.
- USB kabel.
- Účet na [portal.idryer.org](https://portal.idryer.org/) (pro vývoj — [staging.idryer.org](https://staging.idryer.org/)).


REPL je dostupný pouze v buildech s příznakem `IDRYER_DEV_REPL=1`. Otevřete Serial Monitor, zadejte tři příkazy — zařízení se připojí k WiFi, požádá o PIN a je připraveno na claim.


```bash
pio run -e esp32c3-super-mini-dev -t upload
```

Nebo použijte jakýkoliv env, kde je nastaven `-DIDRYER_DEV_REPL=1`.


```bash
pio device monitor -b 115200
```

Po startu uvidíte výzvu:

```
[boot] iDryer dev REPL ready — type 'help'
```

Bezprostředně poté se v logu začnou zobrazovat zprávy cloudového stacku:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```


Zadejte do konzole Serial Monitoru:

```
wifi MyHomeWiFi MySecretPass
```

Odpověď:

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

Přihlašovací údaje jsou zapsány do NVS. Deska okamžitě zavolá `WiFi.begin()`. Log bude ukazovat:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```


Zařízení se automaticky zřídí a zaregistruje 7místný PIN. PIN je platný 10 minut.

1. Otevřete [portal.idryer.org](https://portal.idryer.org/) (nebo staging).
2. Jděte na **Add device**.
3. Zadejte PIN ze Serial Monitoru.

Po úspěšném claimu log ukazuje:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Pokud PIN vypršel dříve, než jste jej zadali — spusťte příkaz `claim` pro získání nového.


| Příkaz | Co dělá | Kdy použít |
|--------|---------|-----------|
| `help` | Zobrazit seznam příkazů | Připomenout si syntaxi |
| `status` | Aktuální stav: WiFi, IP, RSSI, online, serial | Diagnostika připojení |
| `wifi <ssid> <password>` | Uložit WiFi přihlašovací údaje do NVS a reconnect | První onboarding nebo změna sítě |
| `claim` | Ručně spustit tok claimu, získat nový PIN | PIN vypršel nebo je potřeba re-claim |
| `wipe` | Smazat NVS (přihlašovací údaje, claim, menu) a restartovat | Tovární reset |
| `restart` | Software restart ESP | Rychlý restart bez fyzického odpojení |


Improv-WiFi je součástí všech buildů a nezávisí na příznaku `IDRYER_DEV_REPL`. Vhodné pro předání zařízení uživateli nebo když je terminál nepohodlný. Vyžaduje Chrome nebo Edge — Web Serial API není podporován v Safari nebo Firefox.


Jakýkoliv prod build je v pořádku. Improv-WiFi je vždy aktivní.


Jděte na [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/), klikněte na **Connect** a vyberte port USB zařízení v dialogu prohlížeče.


Stránka vás požádá o název sítě a heslo, pošle je do desky přes Serial-Improv. Deska uloží přihlašovací údaje do NVS a připojí se k WiFi. Zřizování a načítání PIN se stane automaticky — stejné jako v Cestě 1.

!!! note
    Improv-WiFi nemůže spustit `claim`, `wipe` nebo zkontrolovat `status`. Pro ruční tok claimu a správu NVS použijte REPL.


| Situace | Doporučení |
|---------|-----------|
| Embedded vývojář s otevřeným terminálem | REPL |
| Předání zařízení uživateli | Improv-WiFi |
| Potřebujete ruční `wipe` nebo repeat `claim` | REPL |
| Prohlížeč Safari nebo Firefox | REPL |
| PlatformIO není nainstalován | Improv-WiFi |


**PIN se v logu nezobrazuje.** Zkontrolujte, zda se zařízení připojilo k WiFi: zadejte `status` a ověřte, že pole `ip=` v odpovědi není prázdné. Zřizování se bez WiFi nespustí.

**PIN vypršel.** Zadejte příkaz `claim` — zařízení si vyžádá novou registraci a vytiskne svěží PIN.

**Zařízení je již připojeno k jinému účtu.** Zadejte `wipe` — NVS bude vymazáno, deska se restartuje a onboarding začne od začátku.

**PIN není portálem přijat.** Ověřte, že jste zkopírovali všech 7 číslic bez mezer a že uplynulo méně než 10 minut od zobrazení PIN.

**Improv-WiFi nevidí zařízení v prohlížeči.** Ujistěte se, že používáte Chrome nebo Edge a že je nainstalován ovladač USB pro ESP32.


- Full Link API: [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Přidejte senzor nebo periférii: [../04-patterns/](../04-patterns/)
