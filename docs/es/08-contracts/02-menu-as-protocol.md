# El menú como protocolo: menu.yaml ↔ mqtt_contract.yaml ↔ portal

---

## Tres archivos, tres papeles

| Archivo | Propietario | Describe |
|------|-------|-----------|
| `src/menu/menu.yaml` | tu producto | menú del dispositivo: parámetros, acciones, estructura |
| `contracts/mqtt_contract.yaml` | idryer-core | lista de significados conocidos: `canonical_roles` con etiquetas en varios idiomas |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | generado | tipos TypeScript para el portal |

**`role:`** — nombre semántico de un elemento del menú. El firmware dice «tengo `iheater.heat_temp`» y no «tengo el elemento número 35». Los nombres internos del firmware pueden cambiar; `role:` se mantiene.

El menú refleja los ajustes del dispositivo. El firmware publica todos los elementos de `menu.yaml`, con o sin `role:`. El portal dibuja cada elemento según su tipo: valor, interruptor, acción, submenú. `role:` da al elemento una etiqueta del contrato en el idioma del usuario; sin `role:`, el portal muestra el nombre que envió el dispositivo.

El menú no construye la tarjeta del dispositivo. Lo que muestra la tarjeta y qué operaciones lanza lo describe el card manifest; ver [Tarjeta del dispositivo: el card manifest](../09-add-product/02-add-widget.md). Una acción de la tarjeta puede tomar los límites y el valor por defecto de su parámetro de un elemento del menú.

---

## 1. Compilación del firmware (`pio run`)

`menu.yaml` → `menu_gen.py` comprueba cada `role:` contra `canonical_roles` del contrato → si una role es desconocida, la compilación falla con un error y la lista de roles válidas → el generador escribe los archivos C++ en `src/menu/`.

## 2. Actualizar TypeScript para el portal (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` genera `mqtt-api.types.ts` y las etiquetas de roles `roles.{lang}.json` → los archivos se copian al portal.

Ejecútalo cuando cambie el contrato. Haz commit del resultado.

## 3. En ejecución: dispositivo ↔ portal

El firmware publica el menú en el topic `config` (lo hace el código del producto con el comando `get_config`; los productos iDryer también al conectarse) → el backend del portal lo guarda → el portal lo obtiene con `GET /devices/:id/menu-config` → cada elemento se dibuja según su tipo `t` (`val`, `tog`, `act`, `sub`); la etiqueta es `canonical_roles[r].labels[lang]`, después la inglesa y después el nombre `n` del dispositivo.

Los parámetros (`min`, `max`, `val`) vienen del propio elemento del menú: el firmware conoce los valores actuales.

El portal cambia un valor con `commands/set { "id": <id>, "val": <value> }`.

---

## Cómo añadir un ajuste (parámetro en NVS)

```yaml
- id: my_param
  type: value
  role: my.param        # opcional: una etiqueta del contrato
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # clave NVS (≤ 15 caracteres)
  persist: true
  scope: global
  default: 50
```

`bind` = clave NVS. `persist: true` = el valor sobrevive a un reinicio.

`role:` no es un campo libre: el valor debe estar en `canonical_roles` del contrato; si no, la compilación falla. La lista está en `contracts/mqtt_contract.yaml` → `canonical_roles` o en `menu.template.yaml`. Una role nueva se añade primero al contrato y después se ejecuta `regen.sh`.

---

## Cómo añadir una operación a la tarjeta del dispositivo

Las operaciones (arrancar, detener, calentar, iluminar) se declaran como acciones de la tarjeta, no como elementos del menú. Sus límites pueden salir de un elemento del menú:

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

La descripción completa: [Tarjeta del dispositivo: el card manifest](../09-add-product/02-add-widget.md).

---

## Qué NO hacer

- No añadas `widget:` a `menu.yaml`. El campo `widget` de `canonical_roles` es solo de referencia: ni el portal ni la aplicación lo leen.
- No edites `mqtt-api.types.ts` a mano: lo genera `regen.sh`.
- No toques los flags `Config.hasXxx` para acciones nuevas: son solo para telemetría (sensores, estados).
