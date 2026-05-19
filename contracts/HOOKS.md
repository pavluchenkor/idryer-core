# Contracts pre-commit hook

This hook protects commits from outdated or invalid `mqtt_contract.yaml`
and generated files.

What it checks (only when relevant files are staged):

1. Runs `regen.sh` as the single pipeline entry point (validate + regenerate).
2. Runs `git diff`: if any generated file differs after regeneration, commit is blocked.

## Install

Run once after cloning (from repository root):

```bash
ln -sf ../contracts/pre_commit.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

Verify:

```bash
ls -la .git/hooks/pre-commit
# -> pre-commit -> ../contracts/pre_commit.sh
```

## Usage

The hook runs automatically on `git commit`. On success, it is silent
(or prints a success message when contracts files are staged).

Scenarios:

### ✅ Everything is OK
```text
$ git add contracts/mqtt_contract.yaml
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

### ❌ YAML is invalid
```text
$ git commit -m "..."
❌ Validation failed:
  • payloads/Foo: required field 'fields' missing

Fix yaml errors above, then commit again.
```
Fix YAML and commit again.

### ❌ Generated file is outdated (forgot to regenerate)
```text
$ git commit -m "..."
❌ _generated/uart_protocol.h is outdated (got regenerated, but differs from what's in repo).

I just regenerated it for you. Review the diff and add it to your commit:

    git diff contracts/_generated/uart_protocol.h
    git add contracts/_generated/uart_protocol.h
    git commit ...
```
The hook regenerates the file. Add it and commit again.

## Bypass (emergency only)

```bash
git commit --no-verify    # skip hook
```

Do not overuse this.

## Remove

```bash
rm .git/hooks/pre-commit
```

## Run Manually (without commit)

```bash
./contracts/pre_commit.sh
```

Useful before a large commit.

## Trigger Files

The hook reacts only to staged changes in:
- `contracts/mqtt_contract.yaml`
- `contracts/_generated/`
- `contracts/gen_*.py`
- `contracts/validate_contract.py`
- `contracts/mqtt_contract.schema.json`
- `contracts/regen.sh`

Commits without these files are skipped.

## Pipeline Components

Related tools in `contracts/`:

| File | Purpose |
|---|---|
| `mqtt_contract.yaml` | Single source of truth for MQTT+UART protocol |
| `mqtt_contract.schema.json` | JSON Schema for YAML structure validation |
| `validate_contract.py` | Validator: schema + cross-refs + kind_id + sizeof |
| `regen.sh` | Runs pipeline (validate + regenerate) |
| `gen_uart_protocol_h.py` | Generates `_generated/uart_protocol.h` (C++) |
| `_generated/uart_protocol.h` | **AUTO-GENERATED**, do not edit manually |

Run directly:
```bash
python3 contracts/validate_contract.py        # validate yaml
python3 contracts/gen_uart_protocol_h.py      # regenerate .h
```
