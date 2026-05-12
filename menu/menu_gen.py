#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор меню v3 (NVS backend).

Отличия от v2:
- Persist через ESP32 Preferences (NVS), а не Arduino EEPROM.
- Ключ NVS = `bind` (global) или `bind_{unit}` (per_controller).
- Лимит NVS key = 15 символов: bind global ≤ 15, bind per_controller ≤ 12
  (чтобы поместился суффикс `_NN`).
- Убраны iDryer-специфичные блоки EEPROM (calibration, errlog) — не нужны
  для ESP32-only проектов.
- MAGIC/VERSION проверяются NVS-ключами `__magic` / `__version`.

Артефакты на выходе:
  menu_ids.h, menu_types.h,
  menu_state.h, menu_state.cpp,
  menu_nvs.h, menu_nvs_io.h, menu_nvs_io.cpp,
  menu_data.cpp,
  menu_bindings.h, menu_bindings.cpp,
  menu_callbacks_weak.cpp,
  menu_presets_autogen.h,
  menu_meta.h,
  menu_cache.h, menu_cache.cpp.

YAML-структура — та же что у v2 (drop-in совместимый).
"""

import os, sys, re, argparse, textwrap, yaml

# NVS key длина
NVS_KEY_MAX = 15
# Резерв под суффикс "_NN" у per_controller (до 99 юнитов)
NVS_KEY_RESERVED_FOR_UNIT = 3

# ---------------------------
# Типы значений
# ---------------------------
VTYPES = {
    "float":  ("float",     "VT_F32", 4),
    "uint16": ("uint16_t",  "VT_U16", 2),
    "uint8":  ("uint8_t",   "VT_U8",  1),
    "int32":  ("int32_t",   "VT_I32", 4),
    "bool":   ("bool",      "VT_BOOL",1),
    "uint32": ("uint32_t",  "VT_U32", 4),
}
MENU_TYPES = {"submenu":"MN_SUBMENU","value":"MN_VALUE","toggle":"MN_TOGGLE","action":"MN_ACTION"}

# ---------------------------
# Утилиты
# ---------------------------
def to_enum_id(s: str) -> str:
    s = re.sub(r'[^a-zA-Z0-9_]+' , '_', s.strip())
    return s.upper()

def collect_langs(node, langs):
    if isinstance(node, dict):
        if "title" in node and isinstance(node["title"], dict):
            langs.update(node["title"].keys())
        if "unit" in node and isinstance(node.get("unit"), dict):
            langs.update(node["unit"].keys())
        for v in node.values():
            collect_langs(v, langs)
    elif isinstance(node, list):
        for x in node:
            collect_langs(x, langs)

def infer_lang_order(langs_set):
    ordered, base = [], ["ru","en"]
    for b in base:
        if b in langs_set:
            ordered.append(b); langs_set.remove(b)
    ordered += sorted(langs_set)
    return ordered

def c_string(s): return '"' + str(s).replace('\\','\\\\').replace('"','\\"') + '"'

def c_float(v):
    s = f"{float(v):.6g}"
    if '.' not in s and 'e' not in s.lower():
        s += ".0"
    return s + "f"

def flatten(nodes, parent_idx=-1, out=None, inherited_scope=None):
    if out is None: out = []
    for n in nodes:
        idx = len(out)
        scope = n.get("scope", inherited_scope or "per_controller")
        entry = {
            "raw": n,
            "parent": parent_idx,
            "first_child": -1,
            "child_count": 0,
            "index": idx,
            "scope": scope
        }
        out.append(entry)
        kids = n.get("children") or []
        if kids:
            out[idx]["first_child"] = len(out)
            out[idx]["child_count"] = len(kids)
            flatten(kids, idx, out, scope)
    return out

def validate_binds(flat):
    """Проверка что bind-имена влезут в NVS ключ."""
    errors = []
    for fn in flat:
        r = fn["raw"]
        bind = r.get("bind")
        if not bind:
            continue
        if r["type"] not in ("value", "toggle"):
            continue
        limit = NVS_KEY_MAX - (NVS_KEY_RESERVED_FOR_UNIT if fn["scope"] != "global" else 0)
        if len(bind) > limit:
            scope_note = "global" if fn["scope"] == "global" else "per_controller (резерв 3 символа под _NN)"
            errors.append(
                f"  - bind '{bind}' (id={r.get('id')}, scope={scope_note}) имеет {len(bind)} символов, лимит {limit}"
            )
    if errors:
        print("ERROR: bind-имена превышают лимит NVS ключа (15 символов).", file=sys.stderr)
        print("\n".join(errors), file=sys.stderr)
        sys.exit(1)

def validate_roles(flat, contract_path=None):
    """Проверяет что все role: в menu.yaml существуют в canonical_roles контракта.

    contract_path — путь к mqtt_contract.yaml. По умолчанию ищет
    ../contracts/mqtt_contract.yaml относительно этого файла.
    Если контракт не найден — валидация пропускается с предупреждением.
    """
    if contract_path is None:
        contract_path = os.path.join(os.path.dirname(__file__), "..", "contracts", "mqtt_contract.yaml")
    contract_path = os.path.normpath(contract_path)

    if not os.path.exists(contract_path):
        print(f"[menu_gen] WARNING: contract not found at {contract_path} — role validation skipped", file=sys.stderr)
        return

    with open(contract_path, encoding="utf-8") as f:
        contract = yaml.safe_load(f)

    known_roles = contract.get("canonical_roles") or {}
    if not known_roles:
        print("[menu_gen] WARNING: canonical_roles not found in contract — role validation skipped", file=sys.stderr)
        return

    errors = []
    for fn in flat:
        role = fn["raw"].get("role")
        if role and role not in known_roles:
            errors.append((role, fn["raw"].get("id", "?")))

    if not errors:
        used = [fn["raw"].get("role") for fn in flat if fn["raw"].get("role")]
        if used:
            print(f"[menu_gen] roles OK — {len(used)} role(s) validated against contract")
        return

    print("ERROR: unknown canonical role(s) in menu.yaml:", file=sys.stderr)
    for role, item_id in errors:
        print(f"  item id={item_id}: role \"{role}\" not in canonical_roles", file=sys.stderr)

    print("\nAvailable canonical roles:", file=sys.stderr)
    for role, spec in sorted(known_roles.items()):
        widget = spec.get("widget", "—") if isinstance(spec, dict) else "—"
        rtype  = spec.get("type",   "—") if isinstance(spec, dict) else "—"
        print(f"  {role:<40} widget: {widget}, type: {rtype}", file=sys.stderr)

    print("\nFix role(s) above or add them to mqtt_contract.yaml → canonical_roles", file=sys.stderr)
    sys.exit(1)


def node_title_unit_arrays(raw, lang_order):
    if isinstance(raw.get("title"), dict):
        titles = [c_string(raw["title"].get(l, raw["title"].get("en",""))) for l in lang_order]
    else:
        t = c_string(raw.get("title",""))
        titles = [t for _ in lang_order]
    uo = raw.get("unit")
    if isinstance(uo, dict):
        units = [c_string(uo.get(l, uo.get("en",""))) for l in lang_order]
    elif isinstance(uo, str):
        u = c_string(uo); units = [u for _ in lang_order]
    else:
        units = ["nullptr" for _ in lang_order]
    return titles, units

def _find_up(filename, start):
    cur = os.path.abspath(start)
    while True:
        cand = os.path.join(cur, filename)
        if os.path.exists(cand):
            return cand
        parent = os.path.dirname(cur)
        if parent == cur:
            return None
        cur = parent

def read_version_major(default=1):
    here = os.path.dirname(__file__)
    vpath = _find_up("version.h", here)
    if not vpath:
        return default
    text = open(vpath, "r", encoding="utf-8").read()
    m = re.search(r"#define\s+VERSION_MAJOR\s+(\d+)", text)
    if m:
        return int(m.group(1))
    m = re.search(r'"(\d+)\.(\d+)\.(\d+)"', text)
    if m:
        return int(m.group(1))
    return default

# ---------------------------
# Генерация файлов
# ---------------------------
def emit_menu_ids_h(path, flat, lang_order):
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated. Do not edit.\n#pragma once\n#include <stdint.h>\n\ntypedef enum {\n")
        for fn in flat:
            f.write(f"  MENU_{to_enum_id(fn['raw']['id'])},\n")
        f.write("  MENU__COUNT\n} MenuId;\n\n")
        f.write("typedef enum {\n")
        for i,l in enumerate(lang_order):
            f.write(f"  LANG_{to_enum_id(l)} = {i},\n")
        f.write("  LANG_COUNT\n} LangId;\n")

def emit_menu_types_h(path):
    with open(path, "w", encoding="utf-8") as f:
        f.write(textwrap.dedent("""
// Auto-generated. Do not edit.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "menu_ids.h"

typedef enum { MN_SUBMENU, MN_ACTION, MN_VALUE, MN_TOGGLE } MenuType;
typedef enum { VT_F32, VT_U16, VT_U8, VT_I32, VT_BOOL, VT_U32} ValueType;

typedef struct {
  ValueType vtype;
  void*     ptr;
  float     minv, maxv, step;
  void    (*on_change)(void*);
  bool      apply_live;
} ValueSpec;

typedef struct MenuItem {
  uint16_t id;
  const char* title[LANG_COUNT];
  const char* unit[LANG_COUNT];
  MenuType type;
  int16_t parent;
  int16_t first_child;
  uint16_t child_count;
  struct {
    struct { void (*invoke)(); } action;
    ValueSpec value;
  } u;
  int16_t ee_offset; // kept for ABI compatibility; always -1 in NVS backend
  uint16_t ee_size;  // kept for ABI compatibility; always 0 in NVS backend
} MenuItem;

extern const MenuItem g_menu[MENU__COUNT];
"""))

def emit_menu_nvs_h(path, namespace, magic, version):
    """Константы NVS backend: namespace, MAGIC, VERSION."""
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated. Do not edit.\n#pragma once\n")
        f.write("// NVS backend — используется ESP32 Preferences.\n\n")
        f.write(f"#define NVS_MENU_NAMESPACE \"{namespace}\"\n")
        f.write(f"#define NVS_MENU_MAGIC      0x{magic:08X}U\n")
        f.write(f"#define NVS_MENU_VERSION    {version}\n\n")
        f.write("// Служебные ключи (префикс __ чтобы не пересекались с bind-ключами).\n")
        f.write("#define NVS_KEY_MAGIC   \"__magic\"\n")
        f.write("#define NVS_KEY_VERSION \"__version\"\n")

def emit_menu_nvs_io(path_h, path_cpp):
    with open(path_h, "w", encoding="utf-8") as f:
        f.write(textwrap.dedent("""
// Auto-generated. Do not edit.
// NVS I/O helpers — шаблоны поверх ESP32 Preferences.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "menu_nvs.h"

// Общий экземпляр Preferences для меню.
// Открывается один раз в menu_nvs_begin(), не требует begin/end на каждый доступ.
extern Preferences g_menu_prefs;

bool menu_nvs_begin();
void menu_nvs_end();

// Чтение значения по ключу. Если ключа нет — out остаётся с исходным значением
// (используется как default), возвращаем false.
template<typename T> inline bool ee_read(const char* key, T& out);

template<> inline bool ee_read<float>(const char* k, float& o){
  if (!g_menu_prefs.isKey(k)) return false;
  o = g_menu_prefs.getFloat(k, o); return true;
}
template<> inline bool ee_read<uint8_t>(const char* k, uint8_t& o){
  if (!g_menu_prefs.isKey(k)) return false;
  o = g_menu_prefs.getUChar(k, o); return true;
}
template<> inline bool ee_read<uint16_t>(const char* k, uint16_t& o){
  if (!g_menu_prefs.isKey(k)) return false;
  o = g_menu_prefs.getUShort(k, o); return true;
}
template<> inline bool ee_read<uint32_t>(const char* k, uint32_t& o){
  if (!g_menu_prefs.isKey(k)) return false;
  o = g_menu_prefs.getUInt(k, o); return true;
}
template<> inline bool ee_read<int32_t>(const char* k, int32_t& o){
  if (!g_menu_prefs.isKey(k)) return false;
  o = g_menu_prefs.getInt(k, o); return true;
}
template<> inline bool ee_read<bool>(const char* k, bool& o){
  if (!g_menu_prefs.isKey(k)) return false;
  o = g_menu_prefs.getBool(k, o); return true;
}

// Безусловная запись (всегда).
template<typename T> inline bool ee_write(const char* key, const T& val);

template<> inline bool ee_write<float>(const char* k, const float& v){
  return g_menu_prefs.putFloat(k, v) > 0;
}
template<> inline bool ee_write<uint8_t>(const char* k, const uint8_t& v){
  return g_menu_prefs.putUChar(k, v) > 0;
}
template<> inline bool ee_write<uint16_t>(const char* k, const uint16_t& v){
  return g_menu_prefs.putUShort(k, v) > 0;
}
template<> inline bool ee_write<uint32_t>(const char* k, const uint32_t& v){
  return g_menu_prefs.putUInt(k, v) > 0;
}
template<> inline bool ee_write<int32_t>(const char* k, const int32_t& v){
  return g_menu_prefs.putInt(k, v) > 0;
}
template<> inline bool ee_write<bool>(const char* k, const bool& v){
  return g_menu_prefs.putBool(k, v) > 0;
}

// Запись только если значение изменилось. Возвращает true если было изменение.
template<typename T>
inline bool ee_store_field(const char* key, const T& val){
  T cur = val;
  bool had = ee_read<T>(key, cur);
  if (had && cur == val) return false;
  ee_write<T>(key, val);
  return true;
}

// Построить NVS-ключ для per_controller поля: "{bind}_{idx}".
// out буфер должен быть >= 16 байт.
inline void menu_nvs_key_per_unit(char* out, size_t cap, const char* bind, uint8_t idx){
  snprintf(out, cap, "%s_%u", bind, (unsigned)idx);
}
"""))
    with open(path_cpp, "w", encoding="utf-8") as f:
        f.write(textwrap.dedent("""
// Auto-generated. Do not edit.
#include "menu_nvs_io.h"

Preferences g_menu_prefs;

bool menu_nvs_begin(){
  return g_menu_prefs.begin(NVS_MENU_NAMESPACE, /*readOnly=*/false);
}

void menu_nvs_end(){
  g_menu_prefs.end();
}
"""))

def emit_menu_state_h(path, flat, num_units):
    by_bind = {}
    for fn in flat:
        r = fn["raw"]
        if r.get("type") in ("value","toggle") and r.get("bind") and r.get("vtype"):
            bind  = r["bind"]
            vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else None)
            ctype = VTYPES[vtype][0]
            scope = fn["scope"]
            dv    = r.get("default", 0)
            if bind not in by_bind:
                by_bind[bind] = (ctype, vtype, scope, dv)
            else:
                c0, v0, s0, d0 = by_bind[bind]
                if s0 != "global" and scope == "global":
                    by_bind[bind] = (ctype, vtype, scope, dv)

    def fmt_default(vtype, ctype, dv):
        if vtype in ("uint16","uint8","int32","uint32"):
            return f"({ctype}){int(dv)}"
        if vtype == "bool":
            return "true" if bool(dv) else "false"
        return c_float(dv)

    lines = []
    lines += [
        "// Auto-generated. Do not edit.",
        "#pragma once",
        "#include <stdint.h>",
        "#include <stdbool.h>",
        "#include \"menu_ids.h\"",
        "",
        "#ifndef NUM_UNITS",
        f"#define NUM_UNITS {num_units}",
        "#endif",
        "",
        "class MenuState {",
        "public:",
    ]
    for bind, (ctype, vtype, scope, dv) in by_bind.items():
        init = fmt_default(vtype, ctype, dv)
        if scope == "global":
            lines.append(f"  {ctype} {bind} = {init};")
        else:
            lines.append(f"  {ctype} {bind}[NUM_UNITS] = {{ {init} }};")
    lines += [
        "",
        "  void initDefaults();   // выставить дефолты из YAML",
        "  void loadFromNVS();    // подхватить значения из NVS",
        "  void saveToNVS();      // записать все поля в NVS (создать namespace)",
        "};",
        "",
        "extern MenuState menu;",
    ]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_state_cpp(path, flat):
    h = []
    h.append("// Auto-generated. Do not edit.")
    h.append("#include <string.h>")
    h.append("#include <stdio.h>")
    h.append("#include \"menu_state.h\"")
    h.append("#include \"menu_types.h\"")
    h.append("#include \"menu_nvs.h\"")
    h.append("#include \"menu_nvs_io.h\"")
    h.append("")
    h.append("MenuState menu;")
    h.append("")

    h.append("void MenuState::initDefaults(){")
    for fn in flat:
        r = fn["raw"]; scope = fn["scope"]
        if r["type"] in ("value", "toggle") and "default" in r:
            vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else None)
            if vtype not in VTYPES: raise SystemExit(f"Unknown vtype for id={r.get('id')}")
            ctype = VTYPES[vtype][0]; bind = r["bind"]; dv = r["default"]
            def scalar(vt, ct, dv):
                if vt in ("uint16","uint8","int32","uint32"): return f"({ct}){int(dv)}"
                if vt == "bool": return "true" if bool(dv) else "false"
                return c_float(dv)
            if scope == "global":
                h.append(f"  this->{bind} = {scalar(vtype, ctype, dv)};")
            else:
                h.append(f"  for (int i=0;i<NUM_UNITS;i++) this->{bind}[i] = {scalar(vtype, ctype, dv)};")
    h.append("}")
    h.append("")

    h.append("void MenuState::loadFromNVS(){")
    h.append("  menu_nvs_begin();")
    h.append("  uint32_t magic = 0, ver = 0;")
    h.append("  ee_read(NVS_KEY_MAGIC, magic);")
    h.append("  ee_read(NVS_KEY_VERSION, ver);")
    h.append("  if (magic != NVS_MENU_MAGIC || ver != (uint32_t)NVS_MENU_VERSION) {")
    h.append("    menu_nvs_end();")
    h.append("    saveToNVS();  // first boot: persist defaults + magic")
    h.append("    return;")
    h.append("  }")
    h.append("  char key[16];")
    h.append("  (void)key;")
    for fn in flat:
        r = fn["raw"]
        if r.get("type") in ("value","toggle") and r.get("persist", False):
            bind = r["bind"]; scope = fn["scope"]
            if scope == "global":
                h.append(f"  ee_read(\"{bind}\", this->{bind});")
            else:
                h.append(f"  for (int i=0;i<NUM_UNITS;i++) {{ menu_nvs_key_per_unit(key, sizeof(key), \"{bind}\", i); ee_read(key, this->{bind}[i]); }}")
    h.append("  menu_nvs_end();")
    h.append("}")
    h.append("")

    h.append("void MenuState::saveToNVS(){")
    h.append("  menu_nvs_begin();")
    h.append("  ee_write(NVS_KEY_MAGIC,   (uint32_t)NVS_MENU_MAGIC);")
    h.append("  ee_write(NVS_KEY_VERSION, (uint32_t)NVS_MENU_VERSION);")
    h.append("  char key[16];")
    h.append("  (void)key;")
    for fn in flat:
        r = fn["raw"]
        if r.get("type") in ("value","toggle") and r.get("persist", False):
            bind = r["bind"]; scope = fn["scope"]
            if scope == "global":
                h.append(f"  ee_store_field(\"{bind}\", this->{bind});")
            else:
                h.append(f"  for (int i=0;i<NUM_UNITS;i++) {{ menu_nvs_key_per_unit(key, sizeof(key), \"{bind}\", i); ee_store_field(key, this->{bind}[i]); }}")
    h.append("  menu_nvs_end();")
    h.append("}")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(h))

def emit_menu_data_cpp(path, flat, lang_order):
    """В NVS backend ee_offset/ee_size всегда -1/0 (поля оставлены для ABI совместимости)."""
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated. Do not edit.\n")
        f.write("#include <stddef.h>\n")
        f.write("#include \"menu_ids.h\"\n")
        f.write("#include \"menu_types.h\"\n")
        f.write("#include \"menu_state.h\"\n\n")

        on_changes = []
        on_invokes = []
        seen_chg, seen_inv = set(), set()
        for fn in flat:
            r = fn["raw"]
            if r["type"] in ("value", "toggle"):
                oc = r.get("on_change")
                if oc and oc not in seen_chg:
                    on_changes.append(oc); seen_chg.add(oc)
            elif r["type"] == "action":
                iv = r.get("on_invoke")
                if iv and iv not in seen_inv:
                    on_invokes.append(iv); seen_inv.add(iv)

        if on_changes or on_invokes:
            f.write('#ifdef __cplusplus\nextern "C" {\n#endif\n')
            for oc in on_changes: f.write(f"void {oc}(void*);\n")
            for iv in on_invokes: f.write(f"void {iv}(void);\n")
            f.write('#ifdef __cplusplus\n}\n#endif\n\n')

        f.write("extern MenuState menu;\n\n")
        f.write("const MenuItem g_menu[MENU__COUNT] = {\n")

        for i, fn in enumerate(flat):
            r = fn["raw"]
            eid = to_enum_id(r["id"])
            titles, units = node_title_unit_arrays(r, lang_order)
            tcode = MENU_TYPES[r["type"]]
            parent = fn["parent"]
            first_child = fn["first_child"]
            child_count = fn["child_count"]

            f.write(f"  [{i}] = {{\n")
            f.write(f"    MENU_{eid}, {{ {', '.join(titles)} }}, {{ {', '.join(units)} }},\n")
            f.write(f"    {tcode}, {parent}, {first_child}, {child_count},\n")

            if r["type"] in ("value", "toggle"):
                vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else None)
                _, vtc, _ = VTYPES[vtype]
                bind = r["bind"]
                minv = float(r.get("min", 0))
                maxv = float(r.get("max", 0))
                step = float(r.get("step", 1))
                onch = r.get("on_change") or "nullptr"
                apply = "true" if (r.get("apply") == "live") else "false"

                f.write(
                    f"    {{ {{ NULL }}, "
                    f"{{ {vtc}, (void*)&menu.{bind}, {minv:.6g}, {maxv:.6g}, {step:.6g}, {onch}, {apply} }} }},\n"
                )
                f.write("    -1, 0\n")
                f.write("  },\n")

            elif r["type"] == "action":
                inv = r.get("on_invoke") or "nullptr"
                f.write(f"    {{ {{ {inv} }}, {{ VT_F32, NULL, 0, 0, 0, NULL, false }} }},\n")
                f.write("    -1, 0\n")
                f.write("  },\n")

            else:  # submenu
                f.write("    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },\n")
                f.write("    -1, 0\n")
                f.write("  },\n")

        f.write("};\n")

def emit_menu_bindings_h(path):
    h = []
    h.append("// AUTO-GENERATED. DO NOT EDIT.")
    h.append("#pragma once")
    h.append("#include <stdint.h>")
    h.append("#include <stdbool.h>")
    h.append('#include "menu_types.h"')
    h.append('#include "menu_state.h"')
    h.append("")
    h.append("typedef void (*MenuOnChangeFn)(void* ctx);")
    h.append("")
    h.append("typedef enum { SCOPE_GLOBAL=0, SCOPE_PER_CONTROLLER=1 } MenuScope;")
    h.append("")
    h.append("typedef struct {")
    h.append("  uint16_t      id;         // MENU_* enum ID")
    h.append("  const char*   bind;       // NVS ключ (global) или префикс (per_controller)")
    h.append("  ValueType     vtype;")
    h.append("  void*         ptr;")
    h.append("  bool          persist;")
    h.append("  MenuOnChangeFn on_change;")
    h.append("  bool          apply_live;")
    h.append("  MenuScope     scope;")
    h.append("} MenuBinding;")
    h.append("")
    h.append("extern const MenuBinding g_bindings[];")
    h.append("extern const uint16_t     g_bindings_count;")
    h.append("")
    h.append("#ifdef __cplusplus")
    h.append('extern "C" {')
    h.append("#endif")
    h.append("uint8_t menu_get_active_controller(void);")
    h.append("#ifdef __cplusplus")
    h.append("}")
    h.append("#endif")
    h.append("")
    h.append("typedef void (*ConfigChangeHookFn)(uint16_t itemId, uint8_t unit, const char* bind);")
    h.append("#ifdef __cplusplus")
    h.append('extern "C" {')
    h.append("#endif")
    h.append("extern ConfigChangeHookFn g_config_change_hook;")
    h.append("void menu_set_config_change_hook(ConfigChangeHookFn hook);")
    h.append("#ifdef __cplusplus")
    h.append("}")
    h.append("#endif")
    h.append("")
    h.append("bool menu_apply_by_bind(const char* bind, float v);")
    h.append("bool menu_read_by_bind(const char* bind, void* out_value);")
    h.append("const MenuBinding* menu_find_bind(const char* bind);")
    h.append("")
    h.append("// Bootstrap sync: проходит по всем биндингам и записывает текущие")
    h.append("// значения MenuState в g_menu_cache. Вызывать один раз на boot")
    h.append("// ПОСЛЕ menu.loadFromNVS() — иначе menu_buildFullJson() отдаст")
    h.append("// дефолтные значения (cache живёт отдельно от MenuState).")
    h.append("void menu_sync_state_to_cache();")
    h.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write('\n'.join(h))

def emit_menu_bindings_cpp(path, flat):
    def vtype_enum(v):
        return {
            "float":"VT_F32","uint16":"VT_U16","uint8":"VT_U8",
            "int32":"VT_I32","bool":"VT_BOOL","uint32":"VT_U32"
        }[v]

    binds = []
    for it in flat:
        r = it["raw"]; scope = it["scope"]
        if r.get("bind"):
            binds.append((r, scope))

    cpp = []
    cpp.append("// AUTO-GENERATED. DO NOT EDIT.")
    cpp.append('#include <string.h>')
    cpp.append('#include <stdio.h>')
    cpp.append('#include "menu_bindings.h"')
    cpp.append('#include "menu_nvs_io.h"')
    cpp.append('#include "menu_cache.h"   // g_menu_cache — sync inside menu_apply_by_bind')
    cpp.append("")
    cpp.append('#ifdef __cplusplus')
    cpp.append('extern "C" {')
    cpp.append('#endif')
    cpp.append("")
    declared_oc = set()
    for r, scope in binds:
        oc = r.get("on_change")
        if oc and oc not in declared_oc:
            cpp.append(f'void {oc}(void* ctx);')
            declared_oc.add(oc)
    cpp.append("#ifdef __cplusplus")
    cpp.append("}")
    cpp.append("#endif")
    cpp.append("")

    cpp.append("extern MenuState menu;")
    cpp.append("const MenuBinding g_bindings[] = {")
    for r, scope in binds:
        ve = vtype_enum(r["vtype"])
        persist = "true" if r.get("persist") else "false"
        ocp = r.get("on_change") or "nullptr"
        al  = "true" if (r.get("apply","") == "live") else "false"
        scope_enum = "SCOPE_GLOBAL" if scope=="global" else "SCOPE_PER_CONTROLLER"
        menu_id_enum = f"MENU_{to_enum_id(r['id'])}"
        cpp.append(f'  {{{menu_id_enum}, "{r["bind"]}", {ve}, (void*)&menu.{r["bind"]}, {persist}, {ocp}, {al}, {scope_enum}}},')
    cpp.append("};")
    cpp.append("const uint16_t g_bindings_count = sizeof(g_bindings)/sizeof(g_bindings[0]);")
    cpp.append("")
    cpp.append("ConfigChangeHookFn g_config_change_hook = nullptr;")
    cpp.append("")
    cpp.append("void menu_set_config_change_hook(ConfigChangeHookFn hook) {")
    cpp.append("  g_config_change_hook = hook;")
    cpp.append("}")
    cpp.append("")
    cpp.append("const MenuBinding* menu_find_bind(const char* bind) {")
    cpp.append("  if (!bind) return nullptr;")
    cpp.append("  for (uint16_t i=0;i<g_bindings_count;i++)")
    cpp.append("    if (strcmp(g_bindings[i].bind, bind) == 0) return &g_bindings[i];")
    cpp.append("  return nullptr;")
    cpp.append("}")
    cpp.append("")
    cpp.append("static inline void store_value(const MenuBinding& b, float v, uint8_t idx) {")
    cpp.append("  switch (b.vtype) {")
    cpp.append("    case VT_F32:  if (b.scope==SCOPE_GLOBAL) *(float*)b.ptr    = v; else ((float*)b.ptr)[idx]    = v; break;")
    cpp.append("    case VT_U16:  if (b.scope==SCOPE_GLOBAL) *(uint16_t*)b.ptr = (uint16_t)v; else ((uint16_t*)b.ptr)[idx] = (uint16_t)v; break;")
    cpp.append("    case VT_U8:   if (b.scope==SCOPE_GLOBAL) *(uint8_t*)b.ptr  = (uint8_t)v;  else ((uint8_t*)b.ptr)[idx]  = (uint8_t)v;  break;")
    cpp.append("    case VT_I32:  if (b.scope==SCOPE_GLOBAL) *(int32_t*)b.ptr  = (int32_t)v;  else ((int32_t*)b.ptr)[idx]  = (int32_t)v;  break;")
    cpp.append("    case VT_BOOL: if (b.scope==SCOPE_GLOBAL) *(bool*)b.ptr     = (bool)(v!=0.0f); else ((bool*)b.ptr)[idx]     = (bool)(v!=0.0f); break;")
    cpp.append("    case VT_U32:  if (b.scope==SCOPE_GLOBAL) *(uint32_t*)b.ptr = (uint32_t)v; else ((uint32_t*)b.ptr)[idx] = (uint32_t)v; break;")
    cpp.append("  }")
    cpp.append("}")
    cpp.append("")
    cpp.append("static inline void read_value(const MenuBinding& b, void* out_value, uint8_t idx) {")
    cpp.append("  switch (b.vtype) {")
    cpp.append("    case VT_F32:  *(float*)out_value    = (b.scope==SCOPE_GLOBAL)? *(float*)b.ptr    : ((float*)b.ptr)[idx]; break;")
    cpp.append("    case VT_U16:  *(uint16_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint16_t*)b.ptr : ((uint16_t*)b.ptr)[idx]; break;")
    cpp.append("    case VT_U8:   *(uint8_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(uint8_t*)b.ptr  : ((uint8_t*)b.ptr)[idx]; break;")
    cpp.append("    case VT_I32:  *(int32_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(int32_t*)b.ptr  : ((int32_t*)b.ptr)[idx]; break;")
    cpp.append("    case VT_BOOL: *(bool*)out_value     = (b.scope==SCOPE_GLOBAL)? *(bool*)b.ptr     : ((bool*)b.ptr)[idx]; break;")
    cpp.append("    case VT_U32:  *(uint32_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint32_t*)b.ptr : ((uint32_t*)b.ptr)[idx]; break;")
    cpp.append("  }")
    cpp.append("}")
    cpp.append("")
    cpp.append("static inline void build_nvs_key(const MenuBinding& b, uint8_t idx, char* out, size_t cap){")
    cpp.append("  if (b.scope == SCOPE_GLOBAL) {")
    cpp.append("    strncpy(out, b.bind, cap - 1);")
    cpp.append("    out[cap - 1] = '\\0';")
    cpp.append("  } else {")
    cpp.append("    snprintf(out, cap, \"%s_%u\", b.bind, (unsigned)idx);")
    cpp.append("  }")
    cpp.append("}")
    cpp.append("")
    cpp.append("bool menu_read_by_bind(const char* bind, void* out_value) {")
    cpp.append("  const MenuBinding* b = menu_find_bind(bind);")
    cpp.append("  if (!b || !out_value) return false;")
    cpp.append("  uint8_t idx = 0;")
    cpp.append("  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();")
    cpp.append("  read_value(*b, out_value, idx);")
    cpp.append("  return true;")
    cpp.append("}")
    cpp.append("")
    cpp.append("bool menu_apply_by_bind(const char* bind, float v) {")
    cpp.append("  const MenuBinding* b = menu_find_bind(bind);")
    cpp.append("  if (!b) return false;")
    cpp.append("  uint8_t idx = 0;")
    cpp.append("  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();")
    cpp.append("  store_value(*b, v, idx);")
    cpp.append("")
    cpp.append("  // Sync into g_menu_cache so menu_buildFullJson() returns the fresh value.")
    cpp.append("  // Cache lives separately from MenuState — without this sync the")
    cpp.append("  // re-published config after commands/set would show stale data.")
    cpp.append("  {")
    cpp.append("    float cached = 0.0f;")
    cpp.append("    switch (b->vtype) {")
    cpp.append("      case VT_F32:  cached = (b->scope==SCOPE_GLOBAL) ? *(const float*)b->ptr    : ((const float*)b->ptr)[idx]; break;")
    cpp.append("      case VT_U16:  cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const uint16_t*)b->ptr : ((const uint16_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U8:   cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const uint8_t*)b->ptr  : ((const uint8_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_I32:  cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const int32_t*)b->ptr  : ((const int32_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_BOOL: cached = ((b->scope==SCOPE_GLOBAL) ? *(const bool*)b->ptr    : ((const bool*)b->ptr)[idx]) ? 1.0f : 0.0f; break;")
    cpp.append("      case VT_U32:  cached = (float)((b->scope==SCOPE_GLOBAL) ? *(const uint32_t*)b->ptr : ((const uint32_t*)b->ptr)[idx]); break;")
    cpp.append("    }")
    cpp.append("    g_menu_cache.setFloat(b->id, cached, (b->scope==SCOPE_GLOBAL) ? 0 : idx);")
    cpp.append("  }")
    cpp.append("")
    cpp.append("  if (b->persist) {")
    cpp.append("    char key[16];")
    cpp.append("    build_nvs_key(*b, idx, key, sizeof(key));")
    cpp.append("    menu_nvs_begin();")
    cpp.append("    switch (b->vtype) {")
    cpp.append("      case VT_F32:  ee_store_field<float>(   key, (b->scope==SCOPE_GLOBAL)? *(float*)b->ptr    : ((float*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U16:  ee_store_field<uint16_t>(key, (b->scope==SCOPE_GLOBAL)? *(uint16_t*)b->ptr : ((uint16_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U8:   ee_store_field<uint8_t>( key, (b->scope==SCOPE_GLOBAL)? *(uint8_t*)b->ptr  : ((uint8_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_I32:  ee_store_field<int32_t>( key, (b->scope==SCOPE_GLOBAL)? *(int32_t*)b->ptr  : ((int32_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_BOOL: ee_store_field<bool>(    key, (b->scope==SCOPE_GLOBAL)? *(bool*)b->ptr     : ((bool*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U32:  ee_store_field<uint32_t>(key, (b->scope==SCOPE_GLOBAL)? *(uint32_t*)b->ptr : ((uint32_t*)b->ptr)[idx]); break;")
    cpp.append("    }")
    cpp.append("    menu_nvs_end();")
    cpp.append("  }")
    cpp.append("  if (b->on_change) b->on_change((void*)b->ptr);")
    cpp.append("  if (g_config_change_hook) g_config_change_hook(b->id, idx, b->bind);")
    cpp.append("  return true;")
    cpp.append("}")
    cpp.append("")

    # Bootstrap sync MenuState→g_menu_cache (вызывать после menu.loadFromNVS()).
    # Без этого menu_buildFullJson() отдаёт дефолтные значения, не загруженные.
    cpp.append("void menu_sync_state_to_cache() {")
    cpp.append("  for (uint16_t i = 0; i < g_bindings_count; i++) {")
    cpp.append("    const MenuBinding& b = g_bindings[i];")
    cpp.append("    if (b.scope == SCOPE_GLOBAL) {")
    cpp.append("      float v = 0.0f;")
    cpp.append("      switch (b.vtype) {")
    cpp.append("        case VT_F32:  v = *(const float*)b.ptr; break;")
    cpp.append("        case VT_U16:  v = (float)*(const uint16_t*)b.ptr; break;")
    cpp.append("        case VT_U8:   v = (float)*(const uint8_t*)b.ptr; break;")
    cpp.append("        case VT_I32:  v = (float)*(const int32_t*)b.ptr; break;")
    cpp.append("        case VT_BOOL: v = (*(const bool*)b.ptr) ? 1.0f : 0.0f; break;")
    cpp.append("        case VT_U32:  v = (float)*(const uint32_t*)b.ptr; break;")
    cpp.append("      }")
    cpp.append("      g_menu_cache.setFloat(b.id, v, 0);")
    cpp.append("    } else {")
    cpp.append("      // per_controller: копируем NUM_UNITS значений.")
    cpp.append("      for (uint8_t u = 0; u < MENU_MAX_UNITS; u++) {")
    cpp.append("        float v = 0.0f;")
    cpp.append("        switch (b.vtype) {")
    cpp.append("          case VT_F32:  v = ((const float*)b.ptr)[u]; break;")
    cpp.append("          case VT_U16:  v = (float)((const uint16_t*)b.ptr)[u]; break;")
    cpp.append("          case VT_U8:   v = (float)((const uint8_t*)b.ptr)[u]; break;")
    cpp.append("          case VT_I32:  v = (float)((const int32_t*)b.ptr)[u]; break;")
    cpp.append("          case VT_BOOL: v = ((const bool*)b.ptr)[u] ? 1.0f : 0.0f; break;")
    cpp.append("          case VT_U32:  v = (float)((const uint32_t*)b.ptr)[u]; break;")
    cpp.append("        }")
    cpp.append("        g_menu_cache.setFloat(b.id, v, u);")
    cpp.append("      }")
    cpp.append("    }")
    cpp.append("  }")
    cpp.append("}")
    cpp.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write('\n'.join(cpp))

def emit_menu_callbacks_weak_cpp(path, flat):
    on_changes, on_invokes = set(), set()
    for it in flat:
        r = it["raw"]
        oc = r.get("on_change")
        iv = r.get("on_invoke")
        if oc: on_changes.add(oc)
        if iv: on_invokes.add(iv)

    lines = []
    lines.append("// AUTO-GENERATED. DO NOT EDIT.")
    lines.append("// Weak stubs for menu callbacks so linking always succeeds.")
    lines.append("#include <Arduino.h>")
    lines.append("#ifdef __cplusplus")
    lines.append('extern "C" {')
    lines.append("#endif")

    for name in sorted(on_changes):
        lines.append(f"void {name}(void*) __attribute__((weak));")
        lines.append(f"void {name}(void*) {{ /* stub */ }}")

    for name in sorted(on_invokes):
        lines.append(f"void {name}(void) __attribute__((weak));")
        lines.append(f"void {name}(void) {{ /* stub */ }}")

    lines.append("uint8_t menu_get_active_controller(void) __attribute__((weak));")
    lines.append("uint8_t menu_get_active_controller(void) { return 0; }")

    lines.append("#ifdef __cplusplus")
    lines.append("} // extern \"C\"")
    lines.append("#endif")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_presets_h(path, flat):
    lines = []
    lines.append("// AUTO-GENERATED. DO NOT EDIT.")
    lines.append("#pragma once\n")

    seen = set()
    for fn in flat:
        raw = fn["raw"]
        if raw.get("type") != "submenu":
            continue
        pid = str(raw.get("id", ""))
        if not pid.startswith("preset_"):
            continue

        suffix = pid[len("preset_"):]
        bind_temp = None
        bind_time = None
        on_invoke = None

        for ch in raw.get("children", []):
            if not isinstance(ch, dict):
                continue
            ctype = ch.get("type")
            cid = str(ch.get("id", ""))
            if ctype == "value":
                b = ch.get("bind") or cid
                lb, lc = b.lower(), cid.lower()
                if lb.endswith("_temp") or lc.endswith("_temp"):
                    bind_temp = b
                elif lb.endswith("_time") or lc.endswith("_time"):
                    bind_time = b
            elif ctype == "action":
                on_invoke = ch.get("on_invoke")

        if not bind_temp:
            bind_temp = f"preset_{suffix}_temp"
        if not bind_time:
            bind_time = f"preset_{suffix}_time"
        if not on_invoke:
            on_invoke = f"start_drying_{suffix}"

        key = (on_invoke, bind_temp, bind_time)
        if key in seen:
            continue
        seen.add(key)

        title = suffix.upper().replace('_','-')
        lines.append(f"// {title}")
        lines.append(f"DEFINE_PRESET_START({on_invoke}, {bind_temp}, {bind_time})\n")

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_meta_h(path, flat, lang_order):
    lines = []
    lines.append("// Auto-generated for ESP32 LINK. Do not edit.")
    lines.append("// Contains menu metadata only (no pointers to data or callbacks).")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdbool.h>")
    lines.append("")
    lines.append(f"#define MENU_META_COUNT {len(flat)}")
    lines.append(f"#define MENU_LANG_COUNT {len(lang_order)}")
    lines.append("")

    lines.append("typedef enum {")
    lines.append("    META_SUBMENU = 0,")
    lines.append("    META_ACTION = 1,")
    lines.append("    META_VALUE = 2,")
    lines.append("    META_TOGGLE = 3")
    lines.append("} MenuMetaType;")
    lines.append("")

    lines.append("typedef enum {")
    lines.append("    META_VT_F32 = 0,")
    lines.append("    META_VT_U16 = 1,")
    lines.append("    META_VT_U8 = 2,")
    lines.append("    META_VT_I32 = 3,")
    lines.append("    META_VT_BOOL = 4,")
    lines.append("    META_VT_U32 = 5")
    lines.append("} MenuMetaValueType;")
    lines.append("")

    lines.append("typedef enum {")
    lines.append("    META_SCOPE_GLOBAL = 0,")
    lines.append("    META_SCOPE_PER_UNIT = 1")
    lines.append("} MenuMetaScope;")
    lines.append("")

    lines.append("typedef struct {")
    lines.append(f"    uint16_t id;")
    lines.append(f"    const char* title[MENU_LANG_COUNT];")
    lines.append(f"    const char* unit[MENU_LANG_COUNT];")
    lines.append("    MenuMetaType type;")
    lines.append("    int16_t parent;")
    lines.append("    int16_t first_child;")
    lines.append("    uint16_t child_count;")
    lines.append("    MenuMetaValueType vtype;")
    lines.append("    float min_val;")
    lines.append("    float max_val;")
    lines.append("    float step;")
    lines.append("    MenuMetaScope scope;")
    lines.append("    // menu_protocol_v1: канонические роли и хардкод-виджеты для портала.")
    lines.append("    // role — стабильное имя из canonical_roles в mqtt_contract.yaml.")
    lines.append("    // widget — override дефолтного UI-компонента (ProfileEditor / RfidWriter / LedPulse).")
    lines.append("    // Оба nullptr для приватных пунктов меню (не публикуются на портал).")
    lines.append("    const char* role;")
    lines.append("    const char* widget;")
    lines.append("} MenuMeta;")
    lines.append("")

    lines.append("static const MenuMeta g_menu_meta[MENU_META_COUNT] = {")

    meta_types = {"submenu": "META_SUBMENU", "action": "META_ACTION", "value": "META_VALUE", "toggle": "META_TOGGLE"}
    meta_vtypes = {"float": "META_VT_F32", "uint16": "META_VT_U16", "uint8": "META_VT_U8",
                   "int32": "META_VT_I32", "bool": "META_VT_BOOL", "uint32": "META_VT_U32"}

    for i, fn in enumerate(flat):
        r = fn["raw"]
        scope = fn.get("scope", "per_controller")
        titles, units = node_title_unit_arrays(r, lang_order)
        mtype = meta_types.get(r["type"], "META_SUBMENU")
        parent = fn["parent"]
        first_child = fn["first_child"]
        child_count = fn["child_count"]

        if r["type"] in ("value", "toggle"):
            vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else "float")
            mvtype = meta_vtypes.get(vtype, "META_VT_F32")
            minv = float(r.get("min", 0))
            maxv = float(r.get("max", 0))
            step = float(r.get("step", 1))
        else:
            mvtype = "META_VT_F32"
            minv = 0.0
            maxv = 0.0
            step = 0.0

        mscope = "META_SCOPE_GLOBAL" if scope == "global" else "META_SCOPE_PER_UNIT"

        # menu_protocol_v1: опциональные поля. nullptr = приватный пункт меню.
        role_lit   = c_string(r["role"])   if r.get("role")   else "nullptr"
        widget_lit = c_string(r["widget"]) if r.get("widget") else "nullptr"

        lines.append(f"    // [{i}] {r['id']}")
        lines.append(f"    {{ {i}, {{ {', '.join(titles)} }}, {{ {', '.join(units)} }},")
        lines.append(f"      {mtype}, {parent}, {first_child}, {child_count},")
        lines.append(f"      {mvtype}, {c_float(minv)}, {c_float(maxv)}, {c_float(step)}, {mscope},")
        lines.append(f"      {role_lit}, {widget_lit} }},")

    lines.append("};")
    lines.append("")

    lines.append("static inline const MenuMeta* menu_meta_get(uint16_t id) {")
    lines.append("    if (id < MENU_META_COUNT) return &g_menu_meta[id];")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_cache_h(path, flat, num_units):
    lines = []
    lines.append("// Auto-generated for ESP32 LINK. Do not edit.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdbool.h>")
    lines.append('#include "menu_meta.h"')
    lines.append("")
    lines.append(f"#define MENU_MAX_UNITS {num_units}")
    lines.append("")

    value_count = sum(1 for fn in flat if fn["raw"]["type"] in ("value", "toggle"))
    lines.append(f"// Total menu items: {len(flat)}, with values: {value_count}")
    lines.append("")

    lines.append("union MenuValue {")
    lines.append("    float    f32;")
    lines.append("    uint32_t u32;")
    lines.append("    int32_t  i32;")
    lines.append("    uint16_t u16;")
    lines.append("    uint8_t  u8;")
    lines.append("    bool     b;")
    lines.append("};")
    lines.append("")

    lines.append("class MenuCache {")
    lines.append("public:")
    lines.append("    uint16_t revision = 0;")
    lines.append("    uint8_t  active_unit = 0;")
    lines.append("    uint8_t  units_count = 1;")
    lines.append("    uint8_t  lang = 0;")
    lines.append("")
    lines.append("    MenuValue values[MENU_META_COUNT][MENU_MAX_UNITS] = {};")
    lines.append("")
    lines.append("    float getFloat(uint16_t id, uint8_t unit = 255) const {")
    lines.append("        if (id >= MENU_META_COUNT) return 0.0f;")
    lines.append("        const MenuMeta* m = &g_menu_meta[id];")
    lines.append("        uint8_t u = (unit == 255) ? active_unit : unit;")
    lines.append("        if (m->scope == META_SCOPE_GLOBAL) u = 0;")
    lines.append("        if (u >= MENU_MAX_UNITS) u = 0;")
    lines.append("        const MenuValue& v = values[id][u];")
    lines.append("        switch (m->vtype) {")
    lines.append("            case META_VT_F32:  return v.f32;")
    lines.append("            case META_VT_U32:  return (float)v.u32;")
    lines.append("            case META_VT_I32:  return (float)v.i32;")
    lines.append("            case META_VT_U16:  return (float)v.u16;")
    lines.append("            case META_VT_U8:   return (float)v.u8;")
    lines.append("            case META_VT_BOOL: return v.b ? 1.0f : 0.0f;")
    lines.append("            default: return 0.0f;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    void setFloat(uint16_t id, float val, uint8_t unit = 255) {")
    lines.append("        if (id >= MENU_META_COUNT) return;")
    lines.append("        const MenuMeta* m = &g_menu_meta[id];")
    lines.append("        uint8_t u = (unit == 255) ? active_unit : unit;")
    lines.append("        if (m->scope == META_SCOPE_GLOBAL) u = 0;")
    lines.append("        if (u >= MENU_MAX_UNITS) u = 0;")
    lines.append("        MenuValue& v = values[id][u];")
    lines.append("        switch (m->vtype) {")
    lines.append("            case META_VT_F32:  v.f32 = val; break;")
    lines.append("            case META_VT_U32:  v.u32 = (uint32_t)val; break;")
    lines.append("            case META_VT_I32:  v.i32 = (int32_t)val; break;")
    lines.append("            case META_VT_U16:  v.u16 = (uint16_t)val; break;")
    lines.append("            case META_VT_U8:   v.u8  = (uint8_t)val; break;")
    lines.append("            case META_VT_BOOL: v.b   = (val != 0.0f); break;")
    lines.append("            default: break;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    bool getBool(uint16_t id, uint8_t unit = 255) const {")
    lines.append("        return getFloat(id, unit) != 0.0f;")
    lines.append("    }")
    lines.append("    int32_t getInt(uint16_t id, uint8_t unit = 255) const {")
    lines.append("        return (int32_t)getFloat(id, unit);")
    lines.append("    }")
    lines.append("    uint8_t getLang() const { return lang; }")
    lines.append("    uint8_t getUnitsCount() const { return units_count; }")
    lines.append("};")
    lines.append("")
    lines.append("extern MenuCache g_menu_cache;")
    lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_cache_cpp(path):
    lines = []
    lines.append("// Auto-generated for ESP32 LINK. Do not edit.")
    lines.append('#include "menu_cache.h"')
    lines.append("")
    lines.append("MenuCache g_menu_cache;")
    lines.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

# ---------------------------
# main
# ---------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("yaml_path", help="menu.yaml")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.dirname(__file__)), "src"), help="output dir (default: lib/idryer-menu/src/)")
    ap.add_argument("--magic", type=lambda x:int(x,0), default=0x4D4E5551, help="NVS magic (default 0x4D4E5551)")
    ap.add_argument("--version", type=int, default=1, help="NVS version int (default); major читается из version.h VERSION_MAJOR")
    ap.add_argument("--num-units", type=int, default=1, help="Количество контроллеров (NUM_UNITS)")
    ap.add_argument("--namespace", default="iheater-menu", help="NVS namespace (default: iheater-menu)")
    args = ap.parse_args()

    if len(args.namespace) > 15:
        print(f"ERROR: --namespace '{args.namespace}' > 15 символов (лимит NVS).", file=sys.stderr)
        sys.exit(1)

    with open(args.yaml_path, "r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh)
    roots = data if isinstance(data, list) else [data]

    langs=set(); collect_langs(roots, langs)
    lang_order = infer_lang_order(langs)

    flat = flatten(roots, -1, [])
    validate_binds(flat)
    validate_roles(flat)

    outdir = args.out
    os.makedirs(outdir, exist_ok=True)

    emit_menu_ids_h(os.path.join(outdir,"menu_ids.h"), flat, lang_order)
    emit_menu_types_h(os.path.join(outdir,"menu_types.h"))

    ee_major = read_version_major(default=args.version)
    emit_menu_nvs_h(os.path.join(outdir,"menu_nvs.h"), args.namespace, args.magic, ee_major)

    emit_menu_state_h(os.path.join(outdir,"menu_state.h"), flat, args.num_units)
    emit_menu_state_cpp(os.path.join(outdir,"menu_state.cpp"), flat)

    emit_menu_nvs_io(os.path.join(outdir,"menu_nvs_io.h"), os.path.join(outdir,"menu_nvs_io.cpp"))
    emit_menu_data_cpp(os.path.join(outdir,"menu_data.cpp"), flat, lang_order)

    emit_menu_bindings_h(os.path.join(outdir,"menu_bindings.h"))
    emit_menu_bindings_cpp(os.path.join(outdir,"menu_bindings.cpp"), flat)
    emit_menu_callbacks_weak_cpp(os.path.join(outdir, "menu_callbacks_weak.cpp"), flat)

    emit_menu_presets_h(os.path.join(outdir, "menu_presets_autogen.h"), flat)

    emit_menu_meta_h(os.path.join(outdir, "menu_meta.h"), flat, lang_order)
    emit_menu_cache_h(os.path.join(outdir, "menu_cache.h"), flat, args.num_units)
    emit_menu_cache_cpp(os.path.join(outdir, "menu_cache.cpp"))

    print(f"Generated in {outdir} (NVS backend):")
    print("  menu_ids.h, menu_types.h, menu_nvs.h, menu_nvs_io.h/.cpp,")
    print("  menu_state.h/.cpp, menu_data.cpp,")
    print("  menu_bindings.h/.cpp, menu_callbacks_weak.cpp, menu_presets_autogen.h,")
    print("  menu_meta.h, menu_cache.h/.cpp (LINK)")
    print(f"NVS namespace: '{args.namespace}'  MAGIC=0x{args.magic:08X}  VERSION={ee_major}")
    print(f"NUM_UNITS = {args.num_units}; scope: global/per_controller")

if __name__ == "__main__":
    main()
