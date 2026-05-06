# Публикация через devicePublisher

## Когда использовать

`iDryer::Link` уже содержит два встроенных транспорта: MQTT (cloud) и Local WebSocket (LAN). Для большинства задач дополнительный транспорт не нужен.

Используйте `s_link.devicePublisher()`, если продукт сам формирует payload и должен отправить его одновременно в оба канала — например, при публикации конфигурации меню в ответ на `commands/get_config`.

## Готовый код

```cpp
// main.cpp (фрагмент)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// Публикация произвольного JSON-payload в MQTT и Local WS одним вызовом.
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // продуктовая функция
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

Один вызов `publishConfigRaw` доставляет payload в MQTT-топик `idryer/{serial}/config` и всем активным LAN WS-клиентам. Дополнительных клиентов или топиков создавать не нужно.

## Объяснение

`devicePublisher()` — dual-publish хелпер фасада. Используйте его вместо прямого обращения к `mqttClient()` или `LocalAccess`, если только не требуется публикация в нестандартный топик.

Телеметрия и статус публикуются фасадом автоматически по таймеру — `devicePublisher()` не нужен для этих данных.

## Когда нужен третий транспорт

Добавление третьего канала (BLE, Serial JSON, UART-прокси) — архитектурное расширение фасада, не паттерн рецепт. Для подавляющего большинства устройств это не требуется.

Если всё-таки нужно — точки входа в `lib/idryer-core/src/cloud/` (cloud state machine, MQTT) и `lib/idryer-core/src/` (local access). Перед этим убедитесь, что встроенных MQTT и Local WS недостаточно для вашей задачи.

## Полный пример в репо

`publishFullMenu()` в `iDryer-Storage/src/main.cpp:171` — публикация полного JSON меню через `s_link.devicePublisher()->publishConfigRaw(buf, len)`.
