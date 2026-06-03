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
    gen_dart_types.py
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

PORTAL_CONTRACTS="../../../../iDryerPortal/frontend-v2/src/contracts"
if [ -d "$PORTAL_CONTRACTS" ]; then
    cp _generated/mqtt-api.types.ts "$PORTAL_CONTRACTS/mqtt-api.types.ts"
    echo "→ Copied mqtt-api.types.ts → frontend-v2/src/contracts/"
else
    echo "⚠  Portal not found at expected path — skipping frontend copy"
fi

PORTAL_BACKEND_CONTRACTS="../../../../iDryerPortal/backend/src/contracts"
if [ -d "$(dirname "$PORTAL_BACKEND_CONTRACTS")" ]; then
    mkdir -p "$PORTAL_BACKEND_CONTRACTS"
    cp _generated/mqtt-api.types.ts "$PORTAL_BACKEND_CONTRACTS/mqtt-api.types.ts"
    echo "→ Copied mqtt-api.types.ts → backend/src/contracts/"
else
    echo "⚠  Backend not found at expected path — skipping backend copy"
fi

PORTAL_I18N="../../../../iDryerPortal/frontend-v2/src/i18n"
if [ -d "$PORTAL_I18N" ]; then
    for f in _generated/roles.*.json; do
        [ -f "$f" ] || continue
        cp "$f" "$PORTAL_I18N/$(basename "$f")"
        echo "→ Copied $(basename "$f") → frontend-v2/src/i18n/"
    done
else
    echo "⚠  Portal i18n not found — skipping roles JSON copy"
fi

# Composite-виджеты из contracts/widgets/ удалены 2026-05-27. Концепция
# «виджет = карточка устройства на дашборде» (product-specific React-компонент
# в портале, не часть контракта). См. mqtt_contract.yaml → widgets.

MOBILE_CONTRACTS="../../../../flutter-prj/idryer_app/lib/contracts"
if [ -d "$(dirname "$MOBILE_CONTRACTS")" ]; then
    mkdir -p "$MOBILE_CONTRACTS"
    cp _generated/canonical_roles.dart "$MOBILE_CONTRACTS/canonical_roles.dart"
    cp _generated/invoke_actions.dart  "$MOBILE_CONTRACTS/invoke_actions.dart"
    echo "→ Copied canonical_roles.dart, invoke_actions.dart → idryer_app/lib/contracts/"
else
    echo "⚠  Mobile app not found at expected path — skipping Dart copy"
fi

echo "✅ Contracts pipeline OK"
