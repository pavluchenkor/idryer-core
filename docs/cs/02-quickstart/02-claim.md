Po tomto kroku se vaše zařízení zobrazí ve vašem účtu [portal.idryer.org](https://portal.idryer.org/) se stavem Online. Všechny následné restarty jsou automatické — opakované připojení není potřeba.


Claim je jednorázový postup, ve kterém se ESP32 zaregistruje v cloudu idryer.org a připojí se k vašemu účtu. Zařízení vygeneruje sedmimístný PIN platný 10 minut. PIN zadáte na portálu — připojení je hotovo.

Po claimu se `deviceId` uloží do NVS — jedinečný identifikátor zařízení v cloudu. Při následujících restartech se ESP32 připojí přímo k MQTT, bez opakování toku claimu.


- ESP32 naflashovaný z [Kroku 01](01-wifi.md) a připojený k WiFi
- Účet na [portal.idryer.org](https://portal.idryer.org/)
- USB kabel a otevřený Serial Monitor


**1. Ověřte, že sketch obsahuje auto-claim.** Následující řádek musí být v `setup()` (je již přítomen v příkladu `03_with_improv`):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Tento callback se spustí automaticky, když zařízení dosáhne internetu a detekuje, že ještě není připojeno.

**2. Otevřete Serial Monitor** a restartujte desku:

```bash
pio device monitor -b 115200
```

**3. Čekejte na PIN v logu.** Po WiFi → provisioning → awaiting claim:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

Zařízení čeká. PIN je platný 10 minut.

**4. Jděte na [portal.idryer.org](https://portal.idryer.org/)** a přejděte na **Add device**.

**5. Zadejte PIN** ze Serial Monitoru (7 číslic, bez mezer).

**6. Potvrďte připojení** na portálu. Serial Monitor pak zobrazí:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```


Otevřete seznam zařízení na portálu — zařízení se by mělo zobrazit se stavem **Online**. Vestavěná LED začne blikat jednou za 500 ms (pokud používáte příklad `01_blink_status`).

!!! note
    Pokud PIN vypršel (uplynulo více než 10 minut) — restartujte desku. Auto-claim vygeneruje nový PIN.

!!! warning
    Pokud je zařízení již připojeno k jinému účtu, zadejte příkaz `wipe` do Serial Monitoru s povoleným `IDRYER_DEV_REPL=1`. NVS bude vymazáno, deska se restartuje a claim začne od začátku.


- [03-telemetry.md](03-telemetry.md) — připojte senzor a publikujte údaje na portál.
- [02-onboarding.md](02-onboarding.md) — podrobná dokumentace onboardingu pro REPL a Improv cesty.
