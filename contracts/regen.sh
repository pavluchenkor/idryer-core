#!/usr/bin/env bash
# regen.sh — валидирует mqtt_contract.yaml и регенерирует все _generated/*.
#
# Единственная точка запуска для разработчика и pre-commit hook'а.
# Добавлять новый генератор — здесь, в массиве GENERATORS.

set -e

cd "$(dirname "$0")"

echo "→ Validating mqtt_contract.yaml..."
python3 validate_contract.py
echo

GENERATORS=(
    gen_uart_protocol_h.py
    gen_mqtt_topics_h.py
    gen_ts_types.py
    gen_idryer_api_h.py
)

for gen in "${GENERATORS[@]}"; do
    echo "→ Running $gen..."
    python3 "$gen"
    echo
done

echo "✅ Contracts pipeline OK"
