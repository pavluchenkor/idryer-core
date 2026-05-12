# Добавить виджет и поддержку нового устройства

Полный цикл: от форка репозитория до принятого PR. Охватывает firmware, контракт, React-виджет и тестирование на портале.

Если вам нужна только прошивка без нового виджета — см. [01-add-new-product.md](01-add-new-product.md).

---

## Предварительные требования

- Python 3.9+ с `pip install pyyaml jsonschema`
- Node.js 18+
- PlatformIO CLI
- Доступ к порталу iDryer для тестирования UIKit

---

## Шаг 1. Fork и клонирование

1. Форкните репозиторий `idryer-core` на GitHub.
2. Склонируйте форк локально:

    ```bash
    git clone https://github.com/<ваш-ник>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. Убедитесь, что контракт проходит валидацию в текущем состоянии:

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## Шаг 2. Редактирование контракта

Все изменения вносятся в `contracts/mqtt_contract.yaml`. Правила в одном changeset:

!!! warning
    Не редактируйте файлы в `_generated/` — они перезаписываются генераторами.

### 2a. Capability vocabulary (новый тип периферии)

Если устройство имеет новый тип железа (например, CO2-сенсор), добавьте запись в секцию `capability_vocabulary`:

```yaml
capability_vocabulary:
  co2:
    description: "CO2-сенсор (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

Это автоматически добавит поле `hasAirCo2: bool` в `iDryer::Config` при следующей регенерации.

### 2b. Canonical roles (новая роль + виджет)

Если устройство предоставляет новый элемент меню, зарегистрируйте роль в `canonical_roles`:

```yaml
canonical_roles:
  co2.read:
    type: float
    widget: Co2Display
    unit: ppm
    labels:
      ru: "CO₂"
      en: "CO₂"
```

Значение `widget` — имя React-компонента, который вы напишете на шаге 5.

### 2c. Invoke actions (если виджет отправляет команды)

Если виджет вызывает действие на устройстве, добавьте описание в `invoke_actions`:

```yaml
invoke_actions:
  my_device:
    co2.calibrate:
      description: "Запустить калибровку CO2-сенсора"
      args:
        targetPpm:
          type: uint16
          description: "Референсное значение CO2 (ppm)"
          required: true
```

### 2d. Device profile (новый тип устройства)

Добавьте профиль в `device_profiles`:

```yaml
device_profiles:
  my_device:
    description: "Моё устройство"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

Значения `capabilities` берутся из `capability_vocabulary`, определённой на шаге 2a.

---

## Шаг 3. Валидация и регенерация

```bash
cd contracts
./regen.sh
```

Флаги:

| Флаг | Что делает |
|---|---|
| (без флага) | Валидация + все генераторы + копирование на портал |
| `--firmware-only` | Только firmware-генераторы, без копирования на портал |
| `--help` | Справка |

При успехе в `_generated/` обновятся:

- `uart_protocol.h`, `mqtt_topics.h` — C++ заголовки
- `iDryer_api.h` — фасад Config/DeviceType
- `mqtt-api.types.ts` — TypeScript-типы
- `scaffolds/my_device/` — заготовка PlatformIO-проекта
- На портале обновятся файлы в `src/components/widgets/`

Если `regen.sh` завершается с ошибкой, исправьте проблему до продолжения.

---

## Шаг 4. Реализация прошивки

Воспользуйтесь сгенерированным scaffold-проектом:

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

Заполните TODO-секции в `src/main.cpp`:

- `onOnline()` — загрузка конфига из NVS, инициализация железа.
- `loop()` — опрос сенсоров, вызов `s_runtime.publishTelemetry(tel)`.
- `buildInfoJson()` — уже заполнен генератором по capabilities.
- `onInvoke()` — реакция на `co2.calibrate`.

Подробнее — [01-add-new-product.md](01-add-new-product.md).

---

## Шаг 5. Создание React-виджета

Виджеты живут в `contracts/widgets/` и копируются на портал через `regen.sh`.

!!! note
    Не редактируйте виджеты напрямую в `portal/src/components/widgets/` — при следующем запуске `regen.sh` изменения будут перезаписаны. Редактируйте только в `contracts/widgets/`.

### Создать файл виджета

```tsx
// contracts/widgets/Co2Display.tsx
import type { WidgetProps } from "./widget-props";

export function Co2DisplayWidget({ device }: WidgetProps) {
  const unit = device.units[0];
  const co2 = unit?.co2Ppm ?? null;
  return (
    <div style={{ padding: "8px 16px" }}>
      {co2 !== null ? `${co2} ppm` : "—"}
    </div>
  );
}
```

### Зарегистрировать в index.ts

```ts
// contracts/widgets/index.ts
export { Co2DisplayWidget } from "./Co2Display";
```

### Зарегистрировать в widget-registry.tsx (на портале)

После следующего `regen.sh` файл появится в `portal/src/components/widgets/Co2Display.tsx`. Добавьте запись в `widget-registry.tsx` вручную:

```tsx
import { Co2DisplayWidget } from "./Co2Display";

export const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  // ...
  Co2Display: Co2DisplayWidget,
};
```

---

## Шаг 6. Тестирование в UIKit

Откройте `portal/src/pages/UiKitPage.tsx` и добавьте секцию с mock-данными в группу **Device Dashboard Widgets**:

```tsx
<KitSection title="Co2Display">
  <Co2DisplayWidget device={MOCK_DEVICE} item={MOCK_CO2_ITEM} socket={null} />
</KitSection>
```

Откройте портал локально и перейдите на `/uikit` — виджет должен отобразиться без логина.

---

## Шаг 7. PR-чеклист

Перед отправкой PR убедитесь, что:

- [ ] `./contracts/regen.sh` завершается без ошибок
- [ ] `_generated/*` добавлены в коммит (не в `.gitignore`)
- [ ] `contracts/widgets/` — новый файл виджета добавлен
- [ ] `contracts/widgets/index.ts` — виджет экспортирован
- [ ] `widget-registry.tsx` на портале — виджет зарегистрирован
- [ ] Виджет виден в `/uikit` без ошибок в консоли
- [ ] Scaffold в `_generated/scaffolds/my_device/` корректно отражает capabilities
- [ ] Описание PR содержит: что за устройство, какие capabilities, какой виджет

Отправьте PR против ветки `main` репозитория `idryer-core`.

---

## Полный список изменений в одном PR

| Файл | Тип изменения |
|---|---|
| `contracts/mqtt_contract.yaml` | Источник правды |
| `contracts/_generated/*` | Автогенерация — добавляются целиком |
| `contracts/widgets/MyWidget.tsx` | Новый файл |
| `contracts/widgets/index.ts` | +1 строка экспорта |
| *(на портале после `regen.sh`)* | `src/components/widgets/MyWidget.tsx` — копия |
| *(на портале вручную)* | `src/components/widgets/widget-registry.tsx` — +1 запись |
| *(на портале вручную)* | `src/pages/UiKitPage.tsx` — +1 секция в KitGroup |
