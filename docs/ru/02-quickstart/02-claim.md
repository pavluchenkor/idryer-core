# Шаг 02 — Claim: привязка к порталу

После этого шага ваше устройство появится в личном кабинете [portal.idryer.org](https://portal.idryer.org/) со статусом Online. Все последующие перезагрузки — автоматические, повторная привязка не нужна.

## Что такое claim

Claim — одноразовая процедура, при которой ESP32 регистрируется в облаке idryer.org и привязывается к вашему аккаунту. Устройство генерирует 7-значный PIN, действующий 10 минут. Вы вводите PIN в портале — привязка выполнена.

После claiming в NVS сохраняется `deviceId` — уникальный идентификатор устройства в облаке. При следующих перезагрузках ESP32 подключается к MQTT напрямую, без повторного claiming.

## Что понадобится

- ESP32, прошитый из [шага 01](01-wifi.md) и подключённый к WiFi
- Аккаунт на [portal.idryer.org](https://portal.idryer.org/)
- USB-кабель и открытый Serial Monitor

## Шаги

**1. Убедиться, что скетч содержит авто-claim.** В `setup()` должна быть строка (она уже есть в примере `03_with_improv`):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Этот callback вызывается автоматически, когда устройство выходит в интернет и обнаруживает, что ещё не привязано.

**2. Открыть Serial Monitor** и перезагрузить плату:

```bash
pio device monitor -b 115200
```

**3. Дождаться PIN в логе.** После подключения WiFi → provisioning → ожидание claim:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

Устройство ждёт. PIN действителен 10 минут.

**4. Войти на [portal.idryer.org](https://portal.idryer.org/)** и перейти в раздел **Add device**.

**5. Ввести PIN** из Serial Monitor (7 цифр, без пробелов).

**6. Подтвердить привязку** в портале. После этого в Serial Monitor:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

## Проверка

Откройте список устройств на портале — устройство должно отображаться со статусом **Online**. Встроенный LED на плате начнёт мигать раз в 500 мс (если используете пример `01_blink_status`).

!!! note
    Если PIN истёк (прошло больше 10 минут) — перезагрузите плату. Авто-claim сгенерирует новый PIN.

!!! warning
    Если устройство уже привязано к другому аккаунту, введите команду `wipe` в Serial Monitor с включённым `IDRYER_DEV_REPL=1`. NVS сотрётся, плата перезагрузится и начнёт claiming заново.

## Что дальше

- [03-telemetry.md](03-telemetry.md) — подключить датчик и публиковать показания на портал.
- [02-onboarding.md](02-onboarding.md) — подробная документация по onboarding через REPL и Improv.
