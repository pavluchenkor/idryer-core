# Onboarding: первая привязка устройства

Onboarding — одноразовая процедура, при которой ESP32 регистрируется в облаке iDryer и привязывается к вашему аккаунту. После её завершения устройство появляется в портале со статусом Online и состоянием Ready, а все последующие включения — автоматические.

## Что вам понадобится

- ESP32-устройство, прошитое сборкой с REPL: env `esp32c3-super-mini-dev` (см. [Запустить за 5 минут](01-five-minutes.md)) либо любая ваша dev-сборка с флагом `IDRYER_DEV_REPL=1`.
- USB-кабель.
- Аккаунт на [portal.idryer.org](https://portal.idryer.org/) (для разработки — [staging.idryer.org](https://staging.idryer.org/)).

## Путь 1. Через Serial REPL (рекомендуется)

REPL доступен только в сборках с флагом `IDRYER_DEV_REPL=1`. Откройте Serial Monitor, введите три команды — устройство подключится к WiFi, запросит PIN и готово к привязке.

### 1. Прошить dev-сборку

```bash
pio run -e esp32c3-super-mini-dev -t upload
```

Или используйте любой ваш env, в котором задан `-DIDRYER_DEV_REPL=1`.

### 2. Открыть Serial Monitor

```bash
pio device monitor -b 115200
```

После загрузки вы увидите приглашение:

```
[boot] iDryer dev REPL ready — type 'help'
```

Сразу после этого в лог начнут поступать сообщения облачного стека:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```

### 3. Подключить WiFi

Введите в консоли Serial Monitor:

```
wifi MyHomeWiFi MySecretPass
```

Ответ:

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

Креды записываются в NVS. Плата сразу вызывает `WiFi.begin()`. В логе появятся:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

### 4. Получить PIN и привязать в портале

Устройство автоматически провизионируется и регистрирует 7-значный PIN. PIN действителен 10 минут.

1. Откройте [portal.idryer.org](https://portal.idryer.org/) (или staging).
2. Перейдите в раздел **Add device**.
3. Введите PIN из Serial Monitor.

После успешной привязки в логе:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Если PIN истёк до того, как вы успели его ввести — выполните команду `claim` для получения нового.

### Полезные команды REPL

| Команда | Что делает | Когда нужна |
|---------|-----------|-------------|
| `help` | Показать список команд | Напомнить синтаксис |
| `status` | Текущее состояние: WiFi, IP, RSSI, online, serial | Диагностика подключения |
| `wifi <ssid> <password>` | Сохранить WiFi-credentials в NVS и переподключиться | Первый onboarding или смена сети |
| `claim` | Вручную запустить claim flow, получить новый PIN | PIN истёк или нужна повторная привязка |
| `wipe` | Стереть NVS (credentials, claim, меню) и перезагрузить | Сброс к заводским настройкам |
| `restart` | Программная перезагрузка ESP | Быстрый reboot без физического отключения |

## Путь 2. Через Improv-WiFi (Web Serial)

Improv-WiFi встроен во все сборки и не зависит от флага `IDRYER_DEV_REPL`. Подходит для передачи устройства пользователю или когда терминал неудобен. Требует Chrome или Edge — Web Serial API не поддерживается в Safari и Firefox.

### 1. Убедиться, что плата прошита

Подойдёт любая prod-сборка. Improv-WiFi активен всегда.

### 2. Открыть веб-страницу

Перейдите на [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/), нажмите кнопку **Connect** и выберите USB-порт устройства в диалоге браузера.

### 3. Ввести SSID и пароль

Веб-страница запросит имя сети и пароль, передаст их на плату через Serial-Improv. Плата сохранит credentials в NVS и подключится к WiFi. Провизионирование и получение PIN происходят автоматически — так же, как в Пути 1.

!!! note
    Improv-WiFi не умеет выполнять `claim`, `wipe` или смотреть `status`. Для ручного управления claim flow и NVS используйте REPL.

### Когда какой путь использовать

| Ситуация | Рекомендация |
|----------|-------------|
| Embedded-разработчик, терминал открыт | REPL |
| Передаёте устройство пользователю | Improv-WiFi |
| Нужен ручной `wipe` или повтор `claim` | REPL |
| Браузер Safari или Firefox | REPL |
| Нет установленного PlatformIO | Improv-WiFi |

## Если что-то пошло не так

**Не вижу PIN в логе.** Проверьте, что устройство подключилось к WiFi: введите `status` и убедитесь, что в ответе `ip=` не пустой. Без WiFi провизионирование не запускается.

**PIN истёк.** Введите команду `claim` — устройство запросит новую регистрацию и напечатает свежий PIN.

**Устройство уже привязано к другому аккаунту.** Введите `wipe` — NVS сотрётся, плата перезагрузится и начнёт onboarding заново.

**PIN не принимается порталом.** Убедитесь, что скопировали все 7 цифр без пробелов и что с момента появления PIN не прошло 10 минут.

**Improv-WiFi не видит устройство в браузере.** Убедитесь, что используете Chrome или Edge и что USB-драйвер ESP32 установлен.

## Что дальше

- Полный API Link: [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Добавить датчик или периферию: [../04-patterns/](../04-patterns/)
