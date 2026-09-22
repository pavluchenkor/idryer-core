---
title: "Tarjeta del dispositivo: el card manifest"
description: "Cómo un firmware sobre idryer-core describe su tarjeta con s_link.card(): sensores, acciones con parámetros de arranque, qué llega al portal y qué dibujan el portal y la aplicación."
---

# Tarjeta del dispositivo: el card manifest

El portal y la aplicación móvil construyen la tarjeta de cualquier dispositivo a partir de su **card manifest**: una descripción que el firmware publica sobre sí mismo, qué mostrar y qué se puede controlar. Un tipo de dispositivo nuevo no necesita código ni en el portal ni en la aplicación.

El manifiesto forma parte de la fachada `iDryer::Link` (`s_link.card()`). El runtime de bajo nivel (`IdryerRuntime`) no lo publica.

El menú y la tarjeta son cosas distintas. El menú refleja los ajustes del dispositivo: un valor cambiado desde el portal se escribe en la memoria del dispositivo. La tarjeta muestra mediciones y lanza operaciones: los parámetros de arranque van con un comando y no se escriben en el menú. Un parámetro de arranque puede tomar sus límites y su valor por defecto de un elemento del menú.

---

## Cómo funciona

```text
firmware: declaraciones s_link.card()
   │  el core construye el JSON y lo publica retained (QoS 1) en idryer/{key}/card
   ▼
backend del portal: valida el manifiesto (límites, tipos y campos permitidos), lo guarda
   ▼
portal y aplicación: dibujan la tarjeta
   │  el usuario pulsa un botón
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
el core envía "card.<id>" a tu callback
```

El core publica el manifiesto cuando la conexión MQTT está establecida y lo vuelve a publicar cuando cambia la declaración o un elemento del menú del que depende.

---

## Entidades: qué mostrar

Los sensores del vocabulario del ecosistema se añaden según los flags de `Config`, no los declaras:

| Flag de `Config` | Celda de la tarjeta |
|---|---|
| `hasAirTemp` | temperatura del aire |
| `hasAirHumidity` | humedad |
| `hasHeaterTemp` | temperatura del calentador |
| `hasHeater` | potencia del calentador |
| `hasFan` | ventilador encendido / apagado |
| `hasServo` | compuerta abierta / cerrada |
| `hasWeight` | módulos de pesaje (topic `weights`) |
| `hasRfid` | la unidad tiene lector RFID |

Valor propio: añádelo a la telemetría y declara un sensor con su ruta JSON:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Los controles sencillos también son entidades: `button`, `number`, `select`. El valor se envía en cuanto el usuario lo cambia y el core llama a tu callback:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Acciones: operaciones con parámetros de arranque

Una acción es una operación del dispositivo: iniciar el secado, calentar, encender la luz, detener. Tiene un **modo** —el modo de la unidad después de la acción (`status.units[].mode`)— y **parámetros** con un significado (`purpose`). La tarjeta decide qué mostrar según el modo actual de la unidad:

- el modo de la unidad coincide con el modo de una acción → el dispositivo está ocupado con ella: bloque de sesión y botón Detener (la acción con modo `IDLE`);
- si no → el formulario de arranque; varias acciones de arranque → un selector de modo.

Ejemplo: un armario de almacenamiento calefactado con un ESP32. La temperatura objetivo es el elemento del menú `target_temp` (30–50 °C, por defecto 45).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // ya dentro de 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // valores del menú a la caché que lee la tarjeta
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

Qué recibe el callback:

- los números están limitados a los límites de la unidad; un número que falta se sustituye por el valor por defecto;
- `unit` — índice de la unidad a partir de `unitId` (`"U1"` → 0); un comando para una unidad que el dispositivo no tiene se ignora;
- tras el arranque, ajusta `status.mode` y llama a `publishStatusNow()`: con el estado, la tarjeta pasa al bloque de sesión.

### API de acciones

| Llamada | Qué hace |
|---|---|
| `card.action(id, mode, cb)` | una acción; `mode` — modo de la unidad después, `nullptr` — el modo no cambia |
| `.param(id, purpose, MENU_ID)` | un número: límites, paso, valor por defecto y unidad del elemento del menú |
| `.param(id, purpose, min, max, step, def[, unit])` | un número con límites propios |
| `.ceiling(MENU_ID)` | el límite superior del parámetro anterior es el valor de ese elemento del menú (por ejemplo, la temperatura máxima del aire) |
| `.stages(id, "stages", MENU_ID)` | etapas del perfil `[{temperature, ramp, hold}]`, segundos; temperatura de etapa dentro de los límites del elemento del menú |
| `.select(id, purpose, options, count[, def])` | elección de una lista; un valor fuera de la lista se sustituye por el valor por defecto |
| `.color(id, purpose, "#FFFFFF")` | un color `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | botones fijos de la cabecera: localizar el dispositivo, borrar errores |
| `.name("ru", "…").name("en", "…")` | nombre de la acción, cuando la tarjeta no conoce el modo |

Valores de `purpose`: `target_temperature`, `target_humidity`, `duration` (minutos, 0 — sin límite), `stages`, `start_stage` (desde 0), `effect`, `rgb_color`. La tarjeta los usa para las etiquetas de los campos y para funciones del portal: el preset de secado rellena `target_temperature` y `duration` de una acción con modo `DRYING`, un perfil de secado rellena `stages`.

Los callbacks son funciones o lambdas sin capturas. Las cadenas de `name` y las opciones de selección se guardan como punteros: usa literales o arrays estáticos.

---

## Qué llega al portal

El armario de arriba publica (`Config`: `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`):

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` y `default` vienen del elemento del menú. Con `.ceiling()` el parámetro recibe además `max_by`: el título del elemento del menú que recortó el límite superior; la tarjeta lo nombra cuando un valor supera el límite.

---

## Qué dibujan el portal y la aplicación

Esquema, no captura de pantalla. En reposo:

```text
┌─ DIY Storage Cabinet ────────────── [Inactivo] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                   │  ← temperatura, humedad, calentador
│ | 0 %     | apagado         |                   │  ← potencia, ventilador (grises: 0 / apagado)
│ [Temp. 45 °C          ]  [Almacenamiento]       │  ← acción "storage"
└─────────────────────────────────────────────────┘
```

Tras el arranque, el dispositivo informa del modo `STORAGE` y la misma tarjeta muestra el bloque de sesión (temperatura objetivo y tiempo en almacenamiento) y el botón Detener, la acción con modo `IDLE`.

Reglas en ambos lados:

- una celda es gris cuando no hay valor; la potencia, también con 0; el ventilador y la compuerta, en estado apagado / cerrada;
- un valor fuera de los límites no se sustituye: el arranque se bloquea y el motivo aparece bajo el formulario;
- en firmware con acciones en el manifiesto, los botones de localizar y borrar errores existen solo si declara acciones con `identify` / `clear_errors`.

Dónde aparece la tarjeta:

| | Portal | Aplicación |
|---|---|---|
| un tipo de dispositivo que no es un producto iDryer | tarjeta del dashboard desde el manifiesto, con sus acciones; página del dispositivo — lecturas y acciones (si el manifiesto tiene acciones) | inicio — lecturas; acciones — en la página del dispositivo |
| `deviceType = Dryer` | la tarjeta del secador con las mismas acciones del manifiesto | inicio — monitorización; acciones — en la página del dispositivo |

---

## Límites

| | Core | Backend del portal |
|---|---|---|
| entidades | 16 declaradas + automáticas | 32 |
| filas de layout | 8 | 16 |
| ids por fila | 4 | 4 |
| acciones | 8 | 8 |
| parámetros por acción | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| idiomas de `name` | `ru`, `en` | hasta 4 |

`id`: letras latinas minúsculas, dígitos y `_`, hasta 24 caracteres; entidades y acciones comparten un espacio de nombres. El documento del manifiesto está limitado a 4096 bytes; si no cabe, el core escribe `manifest overflows` en el log y no lo publica.

El formato en sí: `contracts/mqtt_contract.yaml`, sección `mqtt_only`, `suffix: card`.
