Po tomto kroku bude váš ESP32 připojen k WiFi a přihlašovací údaje budou uloženy v NVS pro automatické připojení při příštím restartu. Portál a MQTT přijdou v dalším kroku.


**Hardware:**

- Deska ESP32-C3 (DevKit, Super Mini, nebo kompatibilní)
- USB kabel (USB-C nebo Micro-USB v závislosti na vaší desce)

**Software:**

- PlatformIO v VS Code
- Prohlížeč Chrome nebo Edge (Web Serial API není podporován v Safari nebo Firefox)


**1. Vytvořte `platformio.ini`** v kořenu vašeho projektu:

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Nahraďte `board` hodnotou pro vaši desku (`esp32-c3-devkitm-1`, `seeed_xiao_esp32c3`, atd.).

**2. Zkopírujte příklad.** Vezměte obsah [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) a uložte jej jako `src/main.cpp` ve vašem projektu.

**3. Nastavte ChipFamily.** V kopírovaném souboru najděte řádek:

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

Ujistěte se, že ChipFamily odpovídá vašemu čipu: `CF_ESP32_C3`, `CF_ESP32_S3`, nebo `CF_ESP32`.

**4. Flash:**

```bash
pio run -e improv-demo -t upload
```

**5. Otevřete [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)** v prohlížeči Chrome nebo Edge. Klikněte na **Connect** a vyberte port USB zařízení z dialogu prohlížeče.

**6. Zadejte SSID a heslo** pro vaši síť 2,4 GHz. Webová stránka odešle přihlašovací údaje na desku přes Serial-Improv. Deska je uloží do NVS.


Otevřete Serial Monitor:

```bash
pio device monitor -b 115200
```

Po úspěšném připojení uvidíte:

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

Pokud se tento řádek nezobrazí, podívejte se na odkaz na řešení potíží níže.

!!! note
    Pokud jsou přihlašovací údaje již uloženy v NVS z předchozího spuštění, deska se při startu automaticky připojí k WiFi — Improv není potřeba.


- [02-claim.md](02-claim.md) — připojte zařízení k vašemu účtu idryer.org.
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — pokud se WiFi nepřipojí.
