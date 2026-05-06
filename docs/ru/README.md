# idryer-core — документация библиотеки

`idryer-core` — C++ библиотека (Arduino/PlatformIO) для ESP32-устройств iDryer. Управляет WiFi, MQTT, облачной стейт-машиной и маршрутизацией команд. Продукт реализует только специфичное для него поведение.

Это документация **библиотеки**, а не конкретного продукта.
Документация продуктов находится в [`docs/ru/`](../../docs/ru/).

---

## Быстрый старт

**Три вещи, которые вы делаете сами:**

1. Реализуете `IProfile` — пять методов (config, info, loop).
2. Собираете `main.cpp` — статические объекты, передаёте зависимости через конструкторы.
3. Регистрируете `handleCommand` — один обработчик для MQTT и опционально для локального WS.

**Три вещи, которые делает библиотека:**

1. Управляет WiFi → provisioning → MQTT-сессией.
2. Маршрутизирует входящие команды в ваш `handleCommand` (кроме `ping` — он обрабатывается внутри).
3. Вызывает методы вашего `IProfile` в нужные моменты.

**Что можно не трогать:**

- `ArduinoWifiManager`, `ArduinoCredentialStore` и другие `Arduino*`-классы — используйте как есть, без субклассинга.
- `CloudStateMachine` — создайте и передайте в `IdryerRuntime`, дальше она работает сама.
- `ActionDispatcher` — compatibility fallback для invoke/set; для нового продукта обработка команд идёт через `setCommandHandler()`, не через `ActionDispatcher`.

Практический гайд: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Рабочие примеры: [`examples/`](../../examples/)

---

## Разделы

| Раздел | Описание |
|--------|----------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Назначение библиотеки, что она не делает, кто её использует |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Таблица всех модулей: назначение, обязательность |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Короткий вход для нового разработчика: что подключить, прошить, чего ожидать |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | Как продукт собирает стек: порядок создания объектов, паттерн main.cpp |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | Что живёт в библиотеке, что — в продукте |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Поток данных в работающем устройстве: входящие команды, исходящие сообщения, связи |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | Класс `MqttClient`: конструктор, подключение, публикация |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | Все MQTT-топики: строки, payload, retained, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`: что координирует, какие команды обрабатывает |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | UART-бридж для двухпроцессорных устройств |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker: подключение, ограничения |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Arduino-реализации интерфейсов устройства |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | Интерфейс `IProfile`, колбэки, пример `LedStripProfile` |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | Файл `mqtt_contract.yaml`: назначение и правила изменения |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Чеклист сборки нового продукта на базе `idryer-core` |
| [10-troubleshooting](10-troubleshooting.md) | Типовые проблемы: WiFi, provisioning, MQTT, команды, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | Как добавить sensor (источник данных) и его публикацию |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | Как добавить периферия и приём команд |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | Как добавить параллельный transport (BLE, HTTP, custom) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Прикладные рецепты передачи данных между sensors / peripherals / profile / publishers |
