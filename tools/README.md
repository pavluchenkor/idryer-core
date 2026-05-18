# idryer-core / tools

Инструменты для локального тестирования устройств iDryer.

---

## Требования

```bash
pip install pyserial paho-mqtt
```

---

## Конфигурация

Создайте `.env` в корне репы (рядом с `platformio.ini`):

```
LOCAL_EMAIL=you@example.com
LOCAL_PASSWORD=yourpassword
LOCAL_API_URL=http://192.168.1.27:3000
```

Файл уже в `.gitignore` — на GitHub не попадёт.

---

## Полный флоу проверки

### 1. Прошить устройство

```bash
pio run -e esp32c3-super-mini-local -t upload
```

### 2. Подать WiFi (только при первом запуске или после NVS erase)

```bash
python3 lib/idryer-core/tools/improv-test/improv_emulator.py \
  --port /dev/cu.usbmodem... \
  --ssid "MyWiFi" \
  --password "MyPassword"
```

Ждёт сообщения `SUCCESS: device reported PROVISIONED`.

### 3. Клейм на локальном бэкенде

```bash
python3 lib/idryer-core/tools/local_auto_claim.py \
  --port /dev/cu.usbmodem...
```

Читает учётные данные из `.env`. Ждёт WiFi-маркер → шлёт `START_CLAIM` → получает PIN → клеймит через API.

### 4. Проверить обмен данными

```bash
python3 lib/idryer-core/tools/device_smoke_test.py \
  --serial DEVICE_XXXX \
  --broker 192.168.1.27
```

Выхлоп:

```
device: DEVICE_XXXX  broker: 192.168.1.27:1883

[1/3] info ............... OK  fw=1.2.3  (1.2s)
[2/3] telemetry .......... OK  keys=['temp', 'humidity']  (2.4s)
[3/3] get_config ......... OK  keys=['menu', 'wifi']  (0.9s)

────────────────────────────────────────
PASS 3/3
```

---

## Порты устройств

| Устройство    | Порт                        | env                          |
|---------------|-----------------------------|------------------------------|
| idryer-link   | `/dev/cu.usbmodem1143401`   | `esp32c3-super-mini-local`   |
| iHeater-link  | `/dev/cu.usbmodem1143101`   | `esp32c3-super-mini-local`   |
| iDryer-Storage| `/dev/cu.usbmodem1143201`   | `esp32c3-super-mini-local`   |

---

## Скрипты

| Файл | Назначение |
|------|------------|
| `improv-test/improv_emulator.py` | WiFi provisioning через Improv |
| `local_auto_claim.py` | Клейм устройства на локальном бэкенде |
| `device_smoke_test.py` | MQTT smoke test: info / telemetry / get_config |
| `link_test_runner.py` | Расширенный тест iHeater-link (авто-нагрев, интеграции) |
