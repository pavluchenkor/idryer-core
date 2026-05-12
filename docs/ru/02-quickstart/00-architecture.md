# Как устроена idryer-core

idryer-core — библиотека для ESP32, которая берёт на себя весь cloud-стек: WiFi-provisioning через Improv-Serial, claim-протокол привязки к аккаунту idryer.org, MQTT-сессию с TLS и автореконнектом, маршрутизацию команд от портала и периодическую публикацию телеметрии.

Вы пишете только то, что специфично вашему устройству: читаете датчики, управляете периферией. Весь остальной код — внутри библиотеки.

## mqtt_contract.yaml — единственный источник правды

Файл [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) определяет:

- **capabilities** — какая периферия есть у каждого типа устройства (нагреватель, LED-лента, датчик);
- **поля телеметрии** — имена и типы данных в MQTT-пакете;
- **TypeScript-типы** — для портала.

Из этого файла генерируется код автоматически:

| Что генерируется | Куда |
|---|---|
| `iDryer::Config` (has* флаги) | `src/_generated/iDryer_api.h` |
| MQTT topics (C++ константы) | `contracts/_generated/mqtt_topics.h` |
| TypeScript-типы | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Не редактируйте файлы в `src/_generated/` и `contracts/_generated/` вручную — они перезаписываются при следующей регенерации.

## Как добавить новую периферию

Процедура одинакова для любой новой capability — кнопки, датчика CO2, RFID-ридера.

**1.** Добавить запись в `capability_vocabulary` в [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml):

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "Датчик CO2 (ppm)"
```

**2.** Запустить регенерацию:

```bash
cd contracts
./regen.sh
```

После этого в `iDryer::Config` появится поле `hasCo2`, в TypeScript — `HardwareUnitConfigCapabilities.co2`.

**3.** В `main.cpp` устройства установить флаг:

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Прошить устройство. Портал прочитает `co2: true` из MQTT-топика `/info` и отобразит соответствующий UI-блок автоматически.

Для типов периферии, которых нет в контракте, создайте PR в репозиторий idryer-core с добавлением в `capability_vocabulary`. После мержа — `regen.sh`, и поле появится в `Config`.

## Два рабочих продукта

**iDryer Storage Link** — ESP32-C3 с LED-лентой WS2812B и датчиком температуры/влажности SHT31. Управляет подсветкой стеллажа с катушками филамента.

**iHeater Link** — ESP32-C3 с RMT-выходом на нагреватель iHeater. Поддерживает интеграции с Bambu Lab, Klipper/Moonraker и Home Assistant.

Оба продукта подключают idryer-core через PlatformIO `lib_deps` и реализуют только продуктовую логику.

## Что дальше

- [01-wifi.md](01-wifi.md) — подключить ESP32 к WiFi через Improv-Serial.
- [../../../README.md](../../../README.md) — краткий обзор библиотеки и кодогенерации.
