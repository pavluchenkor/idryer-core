#!/usr/bin/env bash
# regen.sh — валидирует mqtt_contract.yaml и регенерирует все _generated/*.
#
# Флаги:
#   (без флагов)      Валидация, все генераторы, копирование в портал
#   --firmware-only   Только firmware-генераторы (C++ headers + scaffold), без копирования в портал
#   --help            Показать эту справку
#
# Единственная точка запуска для разработчика и pre-commit hook'а.
# Добавлять новый генератор — здесь, в массивах FIRMWARE_GENERATORS / ALL_GENERATORS.

FIRMWARE_ONLY=false

for arg in "$@"; do
  case "$arg" in
    --firmware-only) FIRMWARE_ONLY=true ;;
    --help|-h)
      sed -n '2,8p' "$0" | sed 's/^# \?//'
      exit 0
      ;;
  esac
done

set -e

cd "$(dirname "$0")"

echo "→ Validating mqtt_contract.yaml..."
python3 validate_contract.py
echo

FIRMWARE_GENERATORS=(
    gen_uart_protocol_h.py
    gen_mqtt_topics_h.py
    gen_idryer_api_h.py
    gen_scaffold.py
)

ALL_GENERATORS=(
    gen_uart_protocol_h.py
    gen_mqtt_topics_h.py
    gen_ts_types.py
    gen_idryer_api_h.py
    gen_scaffold.py
)

if $FIRMWARE_ONLY; then
    GENERATORS=("${FIRMWARE_GENERATORS[@]}")
else
    GENERATORS=("${ALL_GENERATORS[@]}")
fi

for gen in "${GENERATORS[@]}"; do
    echo "→ Running $gen..."
    python3 "$gen"
    echo
done

if $FIRMWARE_ONLY; then
    echo "✅ Contracts pipeline OK (firmware-only)"
    exit 0
fi

PORTAL_CONTRACTS="$(dirname "$0")/../../../../iDryerPortal/frontend-v2/src/contracts"
if [ -d "$PORTAL_CONTRACTS" ]; then
    cp _generated/mqtt-api.types.ts "$PORTAL_CONTRACTS/mqtt-api.types.ts"
    echo "→ Copied mqtt-api.types.ts → frontend-v2/src/contracts/"
else
    echo "⚠  Portal not found at expected path — skipping frontend copy"
fi

PORTAL_I18N="$(dirname "$0")/../../../../iDryerPortal/frontend-v2/src/i18n"
if [ -d "$PORTAL_I18N" ]; then
    for f in _generated/roles.*.json; do
        [ -f "$f" ] || continue
        cp "$f" "$PORTAL_I18N/$(basename "$f")"
        echo "→ Copied $(basename "$f") → frontend-v2/src/i18n/"
    done
else
    echo "⚠  Portal i18n not found — skipping roles JSON copy"
fi

PORTAL_WIDGETS="$(dirname "$0")/../../../../iDryerPortal/frontend-v2/src/components/widgets"
if [ -d "$PORTAL_WIDGETS" ]; then
    for f in widgets/HeaterControl.tsx widgets/LedPulse.tsx widgets/widget-props.ts widgets/index.ts; do
        [ -f "$f" ] || continue
        cp "$f" "$PORTAL_WIDGETS/$(basename "$f")"
        echo "→ Copied $(basename "$f") → frontend-v2/src/components/widgets/"
    done
else
    echo "⚠  Portal widgets dir not found — skipping widget copy"
fi

echo "✅ Contracts pipeline OK"
