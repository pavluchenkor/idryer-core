# idryer-core

> **Перед тем как что-либо редактировать — прочитайте раздел «Кодогенерация» ниже.**
> Часть файлов в этом репозитории генерируется автоматически, и ваши правки будут перезаписаны.

---

Embedded-библиотека для ESP32-устройств экосистемы iDryer.

Если вы делаете своё устройство, которое должно работать с инфраструктурой [iDryer](https://idryer.org) — облако, портал, мобильное приложение, интеграции с принтерами — эта библиотека снимает с вас всю обвязку: WiFi-provisioning, claim-протокол привязки к аккаунту, MQTT-сессию с TLS и автореконнектом, маршрутизацию команд, периодическую публикацию телеметрии.

Вы пишете только то, что специфично вашему устройству: чтение датчиков, управление периферией, бизнес-логику. Всё остальное — `iDryer::Link link(cfg); link.begin(); link.loop();`.

---

## Кодогенерация

**Единственный источник правды: [`contracts/mqtt_contract.yaml`](contracts/mqtt_contract.yaml)**

Из этого файла автоматически генерируется:

| Что генерируется | Куда | Кто читает |
|---|---|---|
| `iDryer::Config` (has* флаги) | `src/_generated/iDryer_api.h` | Прошивка (`main.cpp`) |
| UART-протокол (structs/enums/kind ids) | `contracts/_generated/uart_protocol.h` | UART bridge |
| MQTT topics (C++ constants) | `contracts/_generated/mqtt_topics.h` | Прошивка |
| `HardwareUnitConfigCapabilities` | `contracts/_generated/mqtt-api.types.ts` | Портал (TypeScript) |

**Правило:** не редактируйте файлы в `src/_generated/` и `contracts/_generated/` вручную — они перезаписываются при следующей регенерации.

### Запуск регенерации

```bash
cd contracts
./regen.sh
```

Внутри: валидация YAML → все генераторы подряд. Занимает ~1 секунду.

Pre-commit hook делает это автоматически. Установка — см. [`contracts/HOOKS.md`](contracts/HOOKS.md).

### Как добавить новую периферию (capability)

Например, добавляем поддержку кнопки (`button`):

**1. Добавить в YAML:**

```yaml
# contracts/mqtt_contract.yaml → capability_vocabulary:
button:
  json_key: "button"
  config_flag: "hasButton"
  description: "Кнопка управления"
```

**2. Запустить регенерацию:**

```bash
cd contracts && ./regen.sh
```

После этого в `iDryer::Config` появится поле `hasButton`, а в TypeScript — `HardwareUnitConfigCapabilities.button`.

**3. В `main.cpp` вашего устройства:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasButton = true,   // ← теперь это поле существует
};
```

**4. Прошить устройство** — портал подхватит `button: true` из `/info` и отобразит нужный UI-блок.

### Навигация по контракту

```bash
cd contracts

# Карта файла
python3 show.py

# Найти конкретный action
python3 show.py invoke_actions.storage_link.led.pulse

# Все invoke actions всех устройств
python3 show.py --actions

# Профили устройств (что умеет каждое)
python3 show.py device_profiles
```

---

## Применение

Используется в реальных устройствах:

- **iDryer Storage Link** — управление подсветкой стеллажа с филаментом.
- **iHeater Link** — мост между принтером (Bambu/Klipper/HA) и нагревательной камерой iHeater.

Каждое устройство — отдельный продуктовый репозиторий, подключающий эту библиотеку через PlatformIO `lib_deps` или симлинк.

## Документация

- Сайт: https://dev.idryer.org/core/ *(после первой публикации)*
- В репозитории: [`docs/ru/`](docs/ru/) — русская версия.

Старт за 5 минут — [`docs/ru/02-quickstart/01-five-minutes.md`](docs/ru/02-quickstart/01-five-minutes.md).

Полный API фасада — [`docs/ru/03-public-api/01-link-api-reference.md`](docs/ru/03-public-api/01-link-api-reference.md).

## Лицензия

[GPL-3.0](LICENSE). Любой продукт, использующий эту библиотеку, обязан публиковать свои исходники под совместимой лицензией.

По вопросам, не покрытым лицензией — связаться с автором: [pavluchenkor](https://github.com/pavluchenkor).
