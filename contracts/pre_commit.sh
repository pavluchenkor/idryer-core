#!/usr/bin/env bash
# Pre-commit hook для contracts pipeline:
#   1. Skip если ничего из contracts не staged.
#   2. Запускает regen.sh (validate + regenerate всё через единую точку).
#   3. Если _generated/* стал отличаться от того, что в репо — отменяет commit.
#
# Установка:   ln -sf ../../lib/idryer-core/contracts/pre_commit.sh .git/hooks/pre-commit
# Удаление:    rm .git/hooks/pre-commit
# Bypass:      git commit --no-verify

set -e

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
CONTRACTS_DIR="$REPO_ROOT/lib/idryer-core/contracts"

# Skip if contracts dir doesn't exist (shallow checkouts, submodule states).
if [ ! -d "$CONTRACTS_DIR" ]; then
    exit 0
fi

# Skip if nothing relevant in this commit.
STAGED_RELEVANT=$(git diff --cached --name-only 2>/dev/null \
    | grep -E "lib/idryer-core/contracts/(mqtt_contract\.yaml|_generated/|gen_.*\.py|validate_contract\.py|mqtt_contract\.schema\.json|regen\.sh)$" \
    || true)
if [ -z "$STAGED_RELEVANT" ]; then
    exit 0
fi

echo "📋 Contracts pre-commit check"
echo "─────────────────────────────"

cd "$CONTRACTS_DIR"

# 1) Validate + regenerate (один источник для всего пайплайна).
if ! ./regen.sh >/tmp/regen_out.txt 2>&1; then
    echo "❌ regen.sh failed:"
    tail -30 /tmp/regen_out.txt
    echo
    echo "Fix yaml errors above, then commit again."
    exit 1
fi

# 2) Sync-check: каждый _generated/* должен совпадать с тем, что в репо.
GENERATED=(
    _generated/uart_protocol.h
    _generated/mqtt_topics.h
    _generated/mqtt-api.types.ts
    ../src/_generated/iDryer_api.h
)

for f in "${GENERATED[@]}"; do
    if ! git diff --exit-code --quiet -- "$f" 2>/dev/null; then
        echo
        echo "❌ $f is stale (got regenerated, but differs from what's in repo)."
        echo
        echo "I just regenerated it for you. Review and add to your commit:"
        echo "    git diff $CONTRACTS_DIR/$f"
        echo "    git add $CONTRACTS_DIR/$f"
        echo "    git commit ..."
        echo
        exit 1
    fi
done

echo
echo "✅ Contracts pipeline OK — commit proceeding."
