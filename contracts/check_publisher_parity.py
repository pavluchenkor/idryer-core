#!/usr/bin/env python3
"""
Проверка чётности каналов: всё, что устройство публикует, должно уходить и в
локальный WS, и в MQTT.

Зачем. Приложение показывает одни и те же карточки в облачном и локальном
режиме. Это возможно ровно потому, что DevicePublisher отдаёт в оба канала один
и тот же документ:

    bool DevicePublisher::publishTelemetry(JsonDocument& doc) {
        wsPublish("telemetry", doc);          // локальный WS
        return mqtt_->publishTelemetry(doc);  // и он же в MQTT
    }

Держится это на дисциплине: достаточно добавить новый publish и забыть строку
wsPublish, чтобы каналы разъехались молча. Наружу это вылезет не сразу — у
человека в локальном режиме просто не будет части данных, и искать причину
придётся по логам телефона. Проверка ловит такое в момент правки.

Исключения бывают: например, результат записи RFID-метки нужен только бэкенду
для корреляции запроса и ответа. Но исключение должно быть осознанным, поэтому
требуется явная пометка в теле метода:

    // parity: mqtt-only — причина

Usage:
    python3 check_publisher_parity.py [path/to/device_publisher.cpp]

Exit codes:
    0 — все публикации идут в оба канала (или помечены как исключение)
    1 — найдена публикация только в один канал без пометки
    2 — файл не найден или не разобрался
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

DEFAULT_SOURCE = Path(__file__).resolve().parent.parent / "src" / "local_access" / "device_publisher.cpp"

# Сигнатура метода публикации: тип возврата, DevicePublisher::publishX(...)
METHOD_RE = re.compile(
    r"^[A-Za-z_][\w:<>\s*&]*\s+DevicePublisher::(publish\w+)\s*\([^)]*\)\s*\{",
    re.MULTILINE,
)

WS_CALL_RE = re.compile(r"\bwsPublish(?:Raw)?\s*\(")
MQTT_CALL_RE = re.compile(r"\bmqtt_->\w+\s*\(")
# Причина обязана стоять в той же строке, что и пометка. Пробелы здесь только
# горизонтальные: с обычным \s* перевод строки съедался, и «причиной»
# становилась следующая строка кода — пометка без причины проходила проверку.
OPT_OUT_RE = re.compile(
    r"//[ \t]*parity:[ \t]*(mqtt-only|ws-only)\b[ \t]*(?:—|-|:)?[ \t]*(.*)"
)

# Вспомогательные методы самого паблишера — они и есть каналы, не публикации.
HELPERS = {"wsPublish", "wsPublishRaw"}


def method_bodies(source: str):
    """Отдаёт (имя, тело) для каждого DevicePublisher::publishX по балансу скобок."""
    for match in METHOD_RE.finditer(source):
        name = match.group(1)
        if name in HELPERS:
            continue
        start = match.end()
        depth = 1
        i = start
        while i < len(source) and depth:
            char = source[i]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
            i += 1
        yield name, source[start : i - 1]


def main(argv: list[str]) -> int:
    path = Path(argv[1]) if len(argv) > 1 else DEFAULT_SOURCE
    if not path.is_file():
        print(f"❌ не найден {path}")
        return 2

    source = path.read_text(encoding="utf-8")
    methods = list(method_bodies(source))
    if not methods:
        print(f"❌ в {path.name} не найдено ни одного DevicePublisher::publish*")
        return 2

    problems: list[str] = []
    exceptions: list[str] = []

    for name, body in methods:
        has_ws = bool(WS_CALL_RE.search(body))
        has_mqtt = bool(MQTT_CALL_RE.search(body))
        opt_out = OPT_OUT_RE.search(body)

        # Пометку разбираем первой. Если сначала отпускать всех, кто шлёт в оба
        # канала, то враньё в пометке проходит молча — и исключение живёт в
        # коде, ничего не исключая.
        if opt_out:
            kind, reason = opt_out.group(1), opt_out.group(2).strip()
            if not reason:
                problems.append(f"{name}: пометка «parity: {kind}» без причины")
                continue
            # Пометка должна совпадать с тем, что метод делает на самом деле.
            actual = (
                "mqtt-only"
                if has_mqtt and not has_ws
                else "ws-only"
                if has_ws and not has_mqtt
                else None
            )
            if actual != kind:
                problems.append(
                    f"{name}: помечен как «{kind}», а на деле "
                    f"{'шлёт в оба канала — пометка не нужна' if actual is None else actual}"
                )
                continue
            exceptions.append(f"{name} ({kind}): {reason}")
            continue

        if has_ws and has_mqtt:
            continue

        if not has_ws and not has_mqtt:
            problems.append(f"{name}: не публикует никуда")
        elif not has_ws:
            problems.append(
                f"{name}: уходит только в MQTT — в локальном режиме этих данных не будет. "
                f"Добавьте wsPublish либо пометку «// parity: mqtt-only — причина»"
            )
        else:
            problems.append(
                f"{name}: уходит только в локальный WS — портал этих данных не увидит. "
                f"Добавьте mqtt_->publish… либо пометку «// parity: ws-only — причина»"
            )

    print(f"📡 Чётность каналов: {path.name}")
    print("─" * 45)
    print(f"Публикаций: {len(methods)}, в оба канала: {len(methods) - len(problems) - len(exceptions)}")
    for line in exceptions:
        print(f"  ⚪ исключение — {line}")

    if problems:
        print()
        for line in problems:
            print(f"  ❌ {line}")
        print()
        print("Каналы обязаны отдавать одно и то же: приложение рисует карточку")
        print("из одних и тех же данных и в облаке, и по локальной сети.")
        return 1

    print("✅ все публикации идут в оба канала")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
