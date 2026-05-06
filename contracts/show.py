#!/usr/bin/env python3
"""
show.py — быстрая навигация по mqtt_contract.yaml.

Выводит YAML-кусок из контракта по dotted path. Без аргументов —
помощь и список top-level секций.

Примеры:

    # карта файла + примеры
    python3 show.py

    # все top-level секции (компактно)
    python3 show.py --list

    # вся секция invoke_actions
    python3 show.py invoke_actions

    # подсекция (storage_link / iheater_link / idryer_link / core / rp2040)
    python3 show.py invoke_actions.storage_link

    # конкретный action — точка в имени поддерживается
    python3 show.py invoke_actions.storage_link.led.pulse

    # список вложенных имён (без содержимого)
    python3 show.py --list invoke_actions

    # плоский список всех invoke actions всех продуктов
    python3 show.py --actions

    # один enum / payload / message
    python3 show.py enums.UartDeviceType
    python3 show.py payloads.Telemetry
    python3 show.py messages.command_drying

Опции:
    --no-color       выключить ANSI подсветку (по умолчанию: вкл если stdout=TTY)
    --no-examples    не дописывать JSON-примеры в конец вывода
"""

import json
import re
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: нужен pyyaml. Установи:  pip install pyyaml", file=sys.stderr)
    sys.exit(2)

CONTRACT = Path(__file__).parent / "mqtt_contract.yaml"


def load_contract() -> dict:
    if not CONTRACT.exists():
        print(f"ERROR: контракт не найден: {CONTRACT}", file=sys.stderr)
        sys.exit(2)
    return yaml.safe_load(CONTRACT.read_text(encoding="utf-8"))


def resolve_path(node, parts):
    """Идём по dotted path. Поддерживает:
       • dict — ключ;
       • list of {name: ...} — поиск по полю name;
       • greedy: на каждом шаге сначала пробуем остаток path как одно имя
         (для имён с точкой типа 'led.pulse', 'device.getConfig').
    Возвращает (node, remaining_parts) — если что-то не разрешилось,
    remaining не пуст.
    """
    while parts:
        full = ".".join(parts)
        # Greedy: вдруг весь остаток — одно имя (led.pulse, device.getConfig)
        if isinstance(node, dict) and full in node:
            return node[full], []
        if isinstance(node, list):
            hit = next((it for it in node
                        if isinstance(it, dict) and it.get("name") == full),
                       None)
            if hit is not None:
                return hit, []
        # Иначе — берём первый сегмент.
        head, parts = parts[0], parts[1:]
        if isinstance(node, dict):
            if head not in node:
                return None, [head] + parts
            node = node[head]
            continue
        if isinstance(node, list):
            hit = next((it for it in node
                        if isinstance(it, dict) and it.get("name") == head),
                       None)
            if hit is None:
                return None, [head] + parts
            node = hit
            continue
        return None, [head] + parts
    return node, []


def list_keys(node) -> list[str]:
    """Имена дочерних элементов (для --list)."""
    if isinstance(node, dict):
        return list(node.keys())
    if isinstance(node, list):
        names = []
        for i, it in enumerate(node):
            if isinstance(it, dict) and "name" in it:
                names.append(str(it["name"]))
            else:
                names.append(f"[{i}]")
        return names
    return []


# Pretty-dumper: для строк с переносами использует literal block scalar (|).
# Без этого yaml.safe_dump переносит длинные/многострочные строки с \-escape,
# что превращает русский multiline-текст в нечитаемую кашу.
class _PrettyDumper(yaml.SafeDumper):
    pass


def _str_representer(dumper, data):
    if "\n" in data:
        # literal block: текст как в редакторе, без \n-экранирования.
        return dumper.represent_scalar("tag:yaml.org,2002:str", data, style="|")
    return dumper.represent_scalar("tag:yaml.org,2002:str", data)


_PrettyDumper.add_representer(str, _str_representer)


def dump_yaml(node) -> str:
    return yaml.dump(
        node,
        Dumper=_PrettyDumper,
        sort_keys=False,
        allow_unicode=True,
        default_flow_style=False,
        width=200,           # длиннее ширина — меньше переносов «по словам»
        indent=2,
    )


# ── ANSI colorizer ────────────────────────────────────────────────────
# Простая построчная подсветка yaml-вывода. Без зависимостей (никакого
# Pygments), регулярки. Включается автоматически если stdout — TTY.

ANSI = {
    "reset":   "\033[0m",
    "bold":    "\033[1m",
    "dim":     "\033[2m",
    "cyan":    "\033[36m",
    "green":   "\033[32m",
    "yellow":  "\033[33m",
    "red":     "\033[31m",
    "magenta": "\033[35m",
    "grey":    "\033[90m",
}

# Значения, для которых подсвечиваем сам value (не ключ) контекстно.
STATUS_COLORS = {
    "implemented":                "green",
    "implemented_backend_only":   "green",
    "implemented_device_only":    "green",
    "implemented_with_app_callback": "green",
    "design_or_partial":          "yellow",
    "explicitly_rejected":        "yellow",
    "disabled":                   "yellow",
    "notes_only":                 "grey",
    "legacy":                     "red",
}

# Ключи, для которых хочется выделить значение в одну строку (status, required и т.п.).
HIGHLIGHT_VALUE_KEYS = {"status", "required", "applicable", "qos", "retained"}


def _supports_color() -> bool:
    return sys.stdout.isatty() and "--no-color" not in sys.argv \
        and not (sys.platform == "win32" and "ANSICON" not in __import__("os").environ)


def _color(text: str, name: str) -> str:
    return f"{ANSI[name]}{text}{ANSI['reset']}"


_RE_KV    = re.compile(r"^(\s*)([\w./_-]+)(:)(\s*)(.*)$")
_RE_LIST  = re.compile(r"^(\s*)(- )(.*)$")
_RE_BLOCK = re.compile(r"\s\|$")          # `key: |` literal block marker


def _colorize_line(line: str) -> str:
    # `key: |`  — выделяем сам маркер `|`.
    if _RE_BLOCK.search(line):
        m = _RE_KV.match(line)
        if m:
            indent, key, colon, sep, val = m.groups()
            return f"{indent}{_color(key, 'cyan')}{colon}{sep}{_color('|', 'magenta')}"

    # `- ` элемент списка.
    m = _RE_LIST.match(line)
    if m:
        indent, dash, rest = m.groups()
        return f"{indent}{_color(dash, 'grey')}{rest}"

    # `key: value`.
    m = _RE_KV.match(line)
    if m:
        indent, key, colon, sep, val = m.groups()
        # Контекстная подсветка значений известных ключей.
        if key in HIGHLIGHT_VALUE_KEYS and val:
            v_strip = val.strip().strip("'\"")
            if v_strip in STATUS_COLORS:
                val = _color(val, STATUS_COLORS[v_strip])
            elif v_strip in ("true", "false", "yes", "no"):
                val = _color(val, "green" if v_strip in ("true", "yes") else "grey")
        return f"{indent}{_color(key, 'cyan')}{colon}{sep}{val}"

    return line


def colorize(text: str) -> str:
    if not _supports_color():
        return text
    return "\n".join(_colorize_line(ln) for ln in text.split("\n"))


# ── JSON-examples extraction ──────────────────────────────────────────
# В контракте payload-примеры лежат в bindings.mqtt.payload.examples
# (либо .example для одиночных). После показа yaml-секции выгребаем их
# и печатаем в виде ready-to-pipe JSON — удобно копи-пастить в mosquitto_pub.

def _walk_examples(node, found):
    """Рекурсивно собирает все examples/example словари из узла."""
    if isinstance(node, dict):
        for k, v in node.items():
            if k == "examples" and isinstance(v, dict):
                for ex_name, ex_payload in v.items():
                    found.append((ex_name, ex_payload))
            elif k == "example" and isinstance(v, dict):
                found.append(("example", v))
            else:
                _walk_examples(v, found)
    elif isinstance(node, list):
        for item in node:
            _walk_examples(item, found)


def format_json_examples(node) -> str:
    found: list[tuple[str, dict]] = []
    _walk_examples(node, found)
    if not found:
        return ""

    use_color = _supports_color()
    header = "─── JSON examples (ready for mosquitto_pub -m '...') ───"
    out = ["", _color(header, "magenta") if use_color else header]
    for name, payload in found:
        title = f"# {name}"
        out.append(_color(title, "yellow") if use_color else title)
        out.append(json.dumps(payload, ensure_ascii=False, indent=2))
        out.append("")
    return "\n".join(out)


def print_help_and_map(doc: dict) -> None:
    print(__doc__.rstrip(), end="\n\n")
    print("Top-level секции в текущем контракте:")
    for k, v in doc.items():
        kind = type(v).__name__
        size = ""
        if isinstance(v, (dict, list)):
            size = f" ({len(v)} {'keys' if isinstance(v, dict) else 'items'})"
        print(f"  • {k:30s}  [{kind}{size}]")


def cmd_actions_flat(doc: dict) -> None:
    """--actions: плоский список всех invoke action'ов."""
    inv = doc.get("invoke_actions") or {}
    if not inv:
        print("(no invoke_actions in contract)")
        return
    for product, acts in inv.items():
        if not isinstance(acts, list):
            continue
        print(f"== {product} ==")
        for a in acts:
            if isinstance(a, dict):
                name = a.get("name", "?")
                status = a.get("status", "?")
                print(f"  {name:30s} [{status}]")
        print()


def main() -> int:
    args = sys.argv[1:]
    doc = load_contract()

    # Без аргументов — help + карта.
    if not args or args[0] in ("-h", "--help"):
        print_help_and_map(doc)
        return 0

    # --actions: плоский список invoke action'ов.
    if args[0] == "--actions":
        cmd_actions_flat(doc)
        return 0

    # --list [path]: только имена (компактно).
    if args[0] == "--list":
        path = args[1] if len(args) > 1 else None
        node = doc if path is None else None
        if path is not None:
            parts = path.replace("/", ".").split(".")
            node, leftover = resolve_path(doc, parts)
            if node is None:
                print(f"ERROR: путь не найден: {path}  (не разрешено: {'.'.join(leftover)})",
                      file=sys.stderr)
                return 1
        for name in list_keys(node):
            print(name)
        return 0

    # Остальное — dotted path к содержимому.
    # Чистим path от опций ("--no-color"/"--no-examples" — позиционно где угодно).
    positional = [a for a in args if not a.startswith("--")]
    if not positional:
        print_help_and_map(doc)
        return 0
    path = positional[0]
    parts = path.replace("/", ".").split(".")
    node, leftover = resolve_path(doc, parts)
    if node is None:
        print(f"ERROR: путь не найден: {path}  (не разрешено: {'.'.join(leftover)})",
              file=sys.stderr)
        sys.exit(1)
    print(colorize(dump_yaml(node)), end="")
    if "--no-examples" not in args:
        examples = format_json_examples(node)
        if examples:
            print(examples, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
