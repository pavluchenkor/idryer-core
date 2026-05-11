# Improv-WiFi эмулятор портала

Python-скрипт, эмулирующий [install.idryer.org](https://install.idryer.org) (и любой Improv-WiFi портал) через USB CDC. Шлёт устройству `WIFI_SETTINGS` RPC и читает ответы по протоколу [Improv-Serial](https://www.improv-wifi.com/serial/).

## Зачем

- Прогнать Improv-flow из терминала без браузера (CI, отладка)
- Воспроизвести «не подключается» баг с фиксированным набором креды/SSID
- Увидеть точный state (`PROVISIONING` / `PROVISIONED` / `STOPPED`) и `ERROR=N` без необходимости открывать DevTools
- Отличить настоящий `PROVISIONED` от false-positive (если `device_url=http://0.0.0.0` — значит WiFi не подключился по-настоящему, IP от DHCP не пришёл)

## Установка

Нужен Python 3 + `pyserial`:

```bash
pip3 install pyserial
```

## Использование

```bash
python3 improv_emulator.py --port /dev/cu.usbmodem11401 --ssid MyWiFi --password mypass
```

Параметры:

| Флаг | По умолчанию | Описание |
|---|---|---|
| `--port` | `/dev/cu.usbmodem11401` | USB-CDC порт устройства |
| `--baud` | `115200` | Скорость (Improv фиксировано 115200) |
| `--ssid` | (обязательно) | SSID сети для подключения |
| `--password` | (обязательно) | Пароль сети |
| `--timeout` | `60` | Секунды ожидания `PROVISIONED` |

## Что показывает

```
[emulator] Opening /dev/cu.usbmodem11401 @ 115200
[emulator] Sending WIFI_SETTINGS ssid='MyWiFi'
[emulator] OK  <- STATE=PROVISIONING
[emulator] OK  <- ERROR=0
[emulator] OK  <- STATE=PROVISIONED
[emulator] OK  <- RPC_RESP cmd=1 data='\x14http://192.168.0.128'
[emulator] SUCCESS: device reported PROVISIONED
```

Exit code: `0` — успех, `1` — таймаут, `2` — ошибка от устройства.

## Частые ошибки

- **`Resource busy`** — порт занят браузером (Chrome WebSerial держит даже после закрытия таба). Закрыть таб install.idryer.org и подождать 5+ секунд, или перезагрузить страницу.
- **`STATE=PROVISIONING` → TIMEOUT** — Improv принял WIFI_SETTINGS, но `WiFi.begin()` не дошёл до `WL_CONNECTED`. Проверить пароль (тест на телефоне), MAC-фильтр, диапазон 2.4 ГГц.
- **`STATE=PROVISIONED` + `device_url=http://0.0.0.0`** — **false-positive**! `WL_CONNECTED` мелькнул транзиентно, но DHCP не прошёл. Реально WiFi не работает. Скорее всего неверный пароль (см. AUTH_FAIL в Improv-WiFi-Library 0.0.4).
- **`STATE=UNKNOWN(0)`** — это `STATE_STOPPED`. Improv-libа поставила его после `tryConnectToWifi` failed.
- **state=None через 30+с** — устройство вообще не отвечает. Загрузка не завершилась (долгий setup), или Improv не настроен в прошивке, или USB CDC мёртв.

## Протокол

Скрипт реализует минимальный Improv-Serial:

- Frame: `IMPROV` + version(1) + type(1=STATE / 3=RPC / 4=RPC_RESP / 2=ERROR) + len + data + checksum
- RPC commands: 1=`WIFI_SETTINGS`, 2=`GET_CURRENT_STATE`, 3=`GET_DEVICE_INFO`, 4=`GET_WIFI_NETWORKS`
- States: 1=`AUTHORIZATION_REQUIRED`, 2=`AUTHORIZED`, 3=`PROVISIONING`, 4=`PROVISIONED`

Эмулятор шлёт только `WIFI_SETTINGS` и парсит все входящие frames.
