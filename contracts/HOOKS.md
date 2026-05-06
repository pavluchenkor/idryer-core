# Contracts pre-commit hook

Этот hook защищает от commit'ов с протухшим или невалидным `mqtt_contract.yaml` /
сгенерированными файлами.

Что проверяет (только если в commit'е есть файлы из `lib/idryer-core/contracts/`):

1. `validate_contract.py` — yaml-валидация: schema, cross-refs, kind_id uniqueness, sizeof.
2. `gen_uart_protocol_h.py` — регенерация `_generated/uart_protocol.h`.
3. `git diff` — если регенерированный файл отличается от того что в репо, commit отменяется.

## Установка

После клона репозитория один раз (запускается из корня `iHeater-link/`):

```bash
ln -sf ../../lib/idryer-core/contracts/pre_commit.sh .git/hooks/pre-commit
```

Проверить:

```bash
ls -la .git/hooks/pre-commit
# → pre-commit -> ../../lib/idryer-core/contracts/pre_commit.sh
```

## Использование

Hook запускается автоматически на `git commit`. При успехе ничего не выводит
(или выводит ✅ когда есть staged contracts-файлы).

Сценарии:

### ✅ Всё ок
```
$ git add lib/idryer-core/contracts/mqtt_contract.yaml
$ git commit -m "chore: tweak rfid event"
📋 Contracts pre-commit check
─────────────────────────────
→ Validating mqtt_contract.yaml...
  ✅ yaml valid
→ Regenerating _generated/uart_protocol.h...
  ✅ generated header in sync

✅ Contracts pipeline OK — commit proceeding.
[main abc1234] chore: tweak rfid event
```

### ❌ yaml невалиден
```
$ git commit -m "..."
❌ Validation failed:
  • payloads/Foo: required field 'fields' missing

Fix yaml errors above, then commit again.
```
→ Правишь yaml, повторяешь.

### ❌ Generated файл протух (забыл регенерировать)
```
$ git commit -m "..."
❌ _generated/uart_protocol.h is stale (got regenerated, but differs from what's in repo).

I just regenerated it for you. Review the diff and add it to your commit:

    git diff lib/idryer-core/contracts/_generated/uart_protocol.h
    git add lib/idryer-core/contracts/_generated/uart_protocol.h
    git commit ...
```
→ Hook сам перегенерил файл. Делаешь `git add` + повторяешь commit.

## Обход (для emergency)

```bash
git commit --no-verify    # пропустить hook полностью
```

Не злоупотреблять — это «защита от дурака», не препятствие.

## Удаление

```bash
rm .git/hooks/pre-commit
```

## Ручной запуск (без commit'а)

```bash
./lib/idryer-core/contracts/pre_commit.sh   # из корня iHeater-link/
# или
cd lib/idryer-core/contracts/ && ./pre_commit.sh
```

Полезно перед коммитом большого изменения, чтобы убедиться что всё пройдёт.

## Какие файлы триггерят проверку

Hook реагирует только на staged изменения в:
- `lib/idryer-core/contracts/mqtt_contract.yaml`
- `lib/idryer-core/contracts/_generated/`
- `lib/idryer-core/contracts/gen_*.py`
- `lib/idryer-core/contracts/validate_contract.py`
- `lib/idryer-core/contracts/mqtt_contract.schema.json`

Любой commit без этих файлов → hook молча пропускает.

## Что ещё в pipeline

Связанные тулзы в `lib/idryer-core/contracts/`:

| Файл | Назначение |
|---|---|
| `mqtt_contract.yaml` | Single source of truth для MQTT+UART протокола |
| `mqtt_contract.schema.json` | JSON Schema валидирует структуру yaml |
| `validate_contract.py` | Validator: schema + cross-refs + kind_id + sizeof |
| `gen_uart_protocol_h.py` | Генерирует `_generated/uart_protocol.h` (C++) |
| `_generated/uart_protocol.h` | **AUTO-GENERATED**, не редактировать руками |

Запуск напрямую:
```bash
cd lib/idryer-core/contracts/
python3 validate_contract.py        # проверить yaml
python3 gen_uart_protocol_h.py      # перегенерировать .h
```
