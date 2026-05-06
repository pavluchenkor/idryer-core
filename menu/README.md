# Menu (NVS-backed) для устройств idryer-core

Один из ключевых блоков ядра. Описание меню — в YAML, всё остальное (C++ структуры,
NVS persist, JSON для облака) генерируется автоматически.

## Что лежит в этой папке

| Файл | Что |
|------|-----|
| `menu_gen.py`         | генератор C++ из YAML. Запускается pre-build хуком, либо руками. |
| `menu.template.yaml`  | шаблон для нового продукта. Скопируй, переименуй, наполни. |
| `README.md`           | этот файл. |

## Как использовать в новом продукте

### 1. Скопировать шаблон

```bash
mkdir -p src/menu
cp lib/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

### 2. Подключить pre-build хук в `platformio.ini`

```ini
[env]
build_flags = -Isrc/menu        ; чтобы код писал #include <menu_state.h>

[env:my-device]
extra_scripts =
  pre:extra_scripts/pre_gen_menu.py     ; pre-hook сам зовёт menu_gen.py
  post:extra_scripts/copy_firmware.py   ; уже есть для prod
```

Сам `pre_gen_menu.py` — образец в `iDryer-Storage/extra_scripts/pre_gen_menu.py`.
Скопируй его 1-в-1, ничего настраивать не надо.

### 3. Наполнить `src/menu/menu.yaml`

Открой шаблон, замени `my_param` / `my_flag` / `mode_*` на свои параметры.
Правила и типы — в шапке самого `menu.template.yaml`.

### 4. `pio run`

При первой сборке pre-hook:
- проверит и поставит `pyyaml` в PIO venv (один раз);
- сгенерирует `src/menu/menu_state.{h,cpp}`, `menu_bindings.{h,cpp}`,
  `menu_cache.{h,cpp}`, `menu_ids.h`, `menu_meta.h` и т.д.

При последующих сборках:
- если `menu.yaml` старее сгенерированных файлов → ничего не делает (`up-to-date`);
- если `menu.yaml` новее (правил руками) → перегенерирует.

## API в продуктовом коде

```cpp
#include <menu_state.h>      // глобальный объект `menu` со всеми bindings
#include <menu_bindings.h>   // menu_apply_by_bind, menu_read_by_bind
#include <menu_commands.h>   // menu_buildFullJson — полный config JSON

menu.my_param = 42;                       // прямой доступ
menu_apply_by_bind("my_param", 42.0f);    // через bind-имя (то же + sync в cache + persist)

bool flag = false;
menu_read_by_bind("my_flag", &flag);

char buf[MENU_FULL_JSON_BUF_SIZE];
size_t len = menu_buildFullJson(buf, sizeof(buf));
// → отправляй в облако через s_link.devicePublisher()->publishConfigRaw(buf, len);
```

## Что лежит в `<product>/src/menu/`

После генерации:

| Файл | Происхождение | Редактировать руками |
|------|---------------|----------------------|
| `menu.yaml`              | source of truth, hand-edit | **да** |
| `menu_commands.{h,cpp}`  | runtime helper (parser/builder), hand-written | да (общий шаблон стащи из любого продукта) |
| `menu_state.{h,cpp}`     | autogen | нет |
| `menu_bindings.{h,cpp}`  | autogen (включает sync-в-cache + NVS persist) | нет |
| `menu_cache.{h,cpp}`     | autogen | нет |
| `menu_ids.h`             | autogen | нет |
| `menu_meta.h`            | autogen | нет |
| `menu_nvs_io.{h,cpp}`    | autogen | нет |
| `menu_data.cpp`          | autogen (defaults) | нет |
| `menu_callbacks_weak.cpp`| autogen (weak hooks) | нет — но можно override в product code |

## Запуск генератора руками

Если хочешь регенерировать без `pio run`:

```bash
python3 lib/idryer-core/menu/menu_gen.py src/menu/menu.yaml --out src/menu --num-units 1
```

`--num-units N` — для устройств с несколькими камерами/юнитами (per_controller scope в yaml).

## Что нельзя нарушать

- `bind` ≤ 15 символов (лимит NVS-ключа).
- `units_count` и `language` — последние два пункта корня (фиксированный контракт с порталом).
- Не редактируй autogen `.h/.cpp` руками. Всё через `menu.yaml` + регенерация.
