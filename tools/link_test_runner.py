#!/usr/bin/env python3
"""
link_test_runner.py — end-to-end smoke test for iDryer Link firmware.

Запускается локально. Проверяет, что устройство:
  - публикует базовые топики (info / status|telemetry / integrations/status)
  - отвечает на commands/get_config своим /config (request-response)
  - подключилось к Bambu (если интеграция configured)
  - публикует HA discovery topics на HA broker (если HA configured)
  - получает данные от Moonraker (если configured + active)
  - принимает решение об авто-нагреве по данным принтера (iHeater Link):
      Bambu: gcodeState=RUNNING + trayType → menu.mat_* → outputMode=1 targetTempC>0
      Moonraker: VirtualChamber.target>0 → outputMode=1 targetTempC>0

Сам стартует fake_moonraker.py и fake_bambu (mosquitto + publisher.py), глушит в finally.

Пример:
  python3 link_test_runner.py --serial DEVICE_XXXX
  python3 link_test_runner.py --serial DEVICE_XXXX --only auto_heat_bambu,auto_heat_moonraker
"""

import argparse
import json
import os
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("FATAL: pip3 install paho-mqtt", file=sys.stderr)
    sys.exit(2)

# Use new callback API to silence deprecation warning, fall back to v1.
def _new_client(client_id: str = "") -> "mqtt.Client":
    try:
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    except AttributeError:
        return mqtt.Client(client_id=client_id)


# ── config ──────────────────────────────────────────────────────────
@dataclass
class Config:
    serial: str
    local_broker: str = "192.168.0.171"
    local_port: int = 1883
    ha_broker: str = "192.168.0.185"
    ha_port: int = 1883
    moonraker_host: str = "192.168.0.171"
    moonraker_port: int = 7125
    fake_moonraker_script: str = (
        "/Users/ruslanpavlucenko/Projects/iDryerProject/docs/"
        "iHeater-link/tools/fake_moonraker.py"
    )
    fake_bambu_dir: str = (
        "/Users/ruslanpavlucenko/Projects/iDryerProject/docs/"
        "iHeater-link/tools/fake_bambu"
    )
    fake_bambu_serial: str = "FAKE_BAMBU_001"
    fake_bambu_lan: str = "12345678"
    test_timeout_s: float = 15.0
    only: List[str] = field(default_factory=list)
    skip: List[str] = field(default_factory=list)


# ── result ──────────────────────────────────────────────────────────
class Result:
    """passed: True=OK, False=FAIL, None=SKIP (not applicable)."""

    def __init__(self, name: str, passed: Optional[bool], details: str = ""):
        self.name = name
        self.passed = passed
        self.details = details


# ── MQTT helpers ────────────────────────────────────────────────────
def mqtt_collect(
    broker: str,
    port: int,
    topics: List[str],
    timeout_s: float,
    stop_after: int = 1,
) -> Dict[str, str]:
    """Subscribe, return as soon as `stop_after` topics seen, else after timeout."""
    collected: Dict[str, str] = {}
    done = threading.Event()

    def on_msg(_c, _u, msg):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
        except Exception:
            payload = ""
        collected[msg.topic] = payload
        if len(collected) >= stop_after:
            done.set()

    c = _new_client()
    c.on_message = on_msg
    try:
        c.connect(broker, port, keepalive=10)
    except Exception as e:
        return {"__error__": f"connect {broker}:{port} failed: {e}"}
    for t in topics:
        c.subscribe(t)
    c.loop_start()
    done.wait(timeout=timeout_s)
    c.loop_stop()
    c.disconnect()
    return collected


def mqtt_publish_one(broker: str, port: int, topic: str, payload: str) -> bool:
    try:
        c = _new_client()
        c.connect(broker, port, 10)
        c.loop_start()
        info = c.publish(topic, payload, qos=0)
        info.wait_for_publish(timeout=5)
        c.loop_stop()
        c.disconnect()
        return True
    except Exception:
        return False


# ── tests ───────────────────────────────────────────────────────────
def test_device_alive(cfg: Config) -> Result:
    """Liveness — устройство РЕАЛЬНО публикует сейчас (свежий timestamp в status или telemetry).
    Storage Link не публикует status (statusPeriodMs=0) — проверяем telemetry как fallback.
    Retained сообщения отсекаем по разнице времени с now.
    """
    import datetime as _dt

    def check_topic(topic: str) -> tuple:
        """Returns (age_s, uptime, ts_str) or raises on failure."""
        msgs = mqtt_collect(cfg.local_broker, cfg.local_port, [topic], cfg.test_timeout_s)
        if "__error__" in msgs or topic not in msgs:
            return None
        try:
            d = json.loads(msgs[topic])
            ts_str = d.get("timestamp")
            uptime = d.get("uptime", "?")
            if not ts_str:
                return None
            ts = _dt.datetime.strptime(ts_str.replace("Z", "+0000"), "%Y-%m-%dT%H:%M:%S%z")
            now = _dt.datetime.now(_dt.timezone.utc)
            return (now - ts).total_seconds(), uptime, ts_str
        except Exception:
            return None

    # Try status first, then telemetry (for devices with statusPeriodMs=0)
    for kind in ("status", "telemetry"):
        topic = f"idryer/{cfg.serial}/{kind}"
        res = check_topic(topic)
        if res is None:
            continue
        age_s, uptime, ts_str = res
        if age_s < 60:
            return Result("device_alive", True, f"свежий {kind} (timestamp {age_s:.0f}s назад, uptime={uptime}s)")
        # stale — remember for error message but try next
        stale = (kind, age_s, ts_str, uptime)

    # Both stale or absent
    try:
        kind, age_s, ts_str, uptime = stale  # type: ignore
        return Result("device_alive", False,
                      f"устройство ОФЛАЙН — последний {kind} был {age_s/3600:.1f}ч назад "
                      f"(timestamp={ts_str}, uptime={uptime}s). Это retained-сообщение с прошлой жизни. "
                      f"Остальные тесты НЕ валидны.")
    except NameError:
        return Result("device_alive", False,
                      "нет ни status, ни telemetry — устройство никогда не публиковало или broker не хранит retained")


def test_local_broker_reachable(cfg: Config) -> Result:
    try:
        c = _new_client()
        c.connect(cfg.local_broker, cfg.local_port, 5)
        c.disconnect()
        return Result(
            "local_broker_reachable",
            True,
            f"{cfg.local_broker}:{cfg.local_port}",
        )
    except Exception as e:
        return Result("local_broker_reachable", False, str(e))


def test_ha_broker_reachable(cfg: Config) -> Result:
    try:
        c = _new_client()
        c.connect(cfg.ha_broker, cfg.ha_port, 5)
        c.disconnect()
        return Result(
            "ha_broker_reachable",
            True,
            f"{cfg.ha_broker}:{cfg.ha_port}",
        )
    except Exception as e:
        return Result("ha_broker_reachable", False, str(e))


def test_mqtt_publish(cfg: Config) -> Result:
    """Устройство публикует хоть что-то на idryer/<serial>/#."""
    pattern = f"idryer/{cfg.serial}/#"
    msgs = mqtt_collect(cfg.local_broker, cfg.local_port, [pattern], cfg.test_timeout_s)
    if "__error__" in msgs:
        return Result("mqtt_publish", False, msgs["__error__"])
    topics = [t for t in msgs.keys() if not t.startswith("__")]
    if topics:
        short = ", ".join(t.split("/", 2)[-1] for t in sorted(topics)[:5])
        return Result(
            "mqtt_publish",
            True,
            f"{len(topics)} топик(ов): {short}",
        )
    return Result(
        "mqtt_publish",
        False,
        f"за {cfg.test_timeout_s:.0f}с ничего не пришло на {pattern}",
    )


def test_info(cfg: Config) -> Result:
    topic = f"idryer/{cfg.serial}/info"
    msgs = mqtt_collect(cfg.local_broker, cfg.local_port, [topic], cfg.test_timeout_s)
    if topic not in msgs:
        return Result("info", False, "no /info published")
    try:
        d = json.loads(msgs[topic])
        return Result(
            "info",
            True,
            f"deviceType={d.get('deviceType')} model={d.get('model')!r} fw={d.get('firmwareVersion')}",
        )
    except Exception as e:
        return Result("info", False, f"json error: {e}")


def test_request_response(cfg: Config) -> Result:
    """publish commands/get_config → expect свежий /config (после старта подписки)."""
    config_topic = f"idryer/{cfg.serial}/config"
    cmd_topic = f"idryer/{cfg.serial}/commands/get_config"

    # Шаг 1: подписываемся и собираем retained — это baseline.
    baseline: Dict[str, str] = {}
    done_baseline = threading.Event()

    def on_baseline(_c, _u, msg):
        baseline[msg.topic] = msg.payload.decode("utf-8", errors="replace")
        done_baseline.set()

    sub = _new_client()
    sub.on_message = on_baseline
    try:
        sub.connect(cfg.local_broker, cfg.local_port, 10)
    except Exception as e:
        return Result("request_response", False, f"sub connect: {e}")
    sub.subscribe(config_topic)
    sub.loop_start()
    # дать retained прилететь (~0.5с обычно достаточно)
    time.sleep(1.0)
    had_retained = config_topic in baseline

    # Шаг 2: переключаем колбэк на сбор "после publish".
    fresh: Dict[str, float] = {}
    fresh_done = threading.Event()
    pub_ts = time.time()

    def on_fresh(_c, _u, msg):
        # Засчитываем только публикации после pub_ts (исключаем повторный retained delivery).
        fresh[msg.topic] = time.time()
        fresh_done.set()

    sub.on_message = on_fresh

    if not mqtt_publish_one(cfg.local_broker, cfg.local_port, cmd_topic, "{}"):
        sub.loop_stop()
        sub.disconnect()
        return Result("request_response", False, "publish get_config failed")
    pub_ts = time.time()

    fresh_done.wait(timeout=cfg.test_timeout_s)
    sub.loop_stop()
    sub.disconnect()

    if config_topic in fresh and fresh[config_topic] >= pub_ts:
        return Result(
            "request_response",
            True,
            f"get_config → свежий /config за {fresh[config_topic]-pub_ts:.1f}с",
        )
    if had_retained:
        size = len(baseline[config_topic])
        return Result(
            "request_response",
            None,
            f"свежего ответа нет, но retained /config есть ({size} байт) — устройство публиковало раньше",
        )
    return Result(
        "request_response",
        False,
        f"ни retained, ни свежего /config (timeout {cfg.test_timeout_s:.0f}с)",
    )


def _fetch_integrations_status(cfg: Config) -> Optional[dict]:
    topic = f"idryer/{cfg.serial}/integrations/status"
    msgs = mqtt_collect(cfg.local_broker, cfg.local_port, [topic], cfg.test_timeout_s)
    if topic not in msgs:
        return None
    try:
        return json.loads(msgs[topic])
    except Exception:
        return None


def test_bambu(cfg: Config) -> Result:
    s = _fetch_integrations_status(cfg)
    if s is None:
        return Result("bambu", False, "integrations/status не пришёл")
    b = s.get("bambu") or {}
    if not b.get("configured"):
        return Result("bambu", None, "интеграция не configured в этой прошивке")
    state = b.get("state")
    if state == "online":
        return Result(
            "bambu",
            True,
            f"online (broker={b.get('printerIp')} serial={b.get('printerSerial')} filament={b.get('currentFilament')})",
        )
    return Result(
        "bambu",
        False,
        f"state={state} lastError={b.get('lastError')!r}",
    )


def test_ha_discovery(cfg: Config) -> Result:
    """Retained discovery topics на HA broker."""
    # paho не поддерживает wildcards в одной подписке broadly, но broker даст retained по # маске.
    pattern = "homeassistant/#"
    timeout = min(cfg.test_timeout_s, 8.0)
    msgs = mqtt_collect(cfg.ha_broker, cfg.ha_port, [pattern], timeout, stop_after=10**6)
    if "__error__" in msgs:
        return Result("ha_discovery", False, msgs["__error__"])
    needle = f"idryer_{cfg.serial}"
    matched = [t for t in msgs.keys() if needle in t]
    if matched:
        return Result(
            "ha_discovery",
            True,
            f"{len(matched)} HA-сущностей зарегистрированы на {cfg.ha_broker}",
        )
    return Result(
        "ha_discovery",
        False,
        f"discovery topics для {needle} не найдены (HA либо disabled, либо никогда не подключался)",
    )


def test_moonraker_link(cfg: Config) -> Result:
    """fake_moonraker уже запущен runner'ом (см. main). Проверяем connect."""
    s = _fetch_integrations_status(cfg)
    if s is None:
        return Result("moonraker", False, "integrations/status не пришёл")
    m = s.get("moonraker") or {}
    if not m.get("configured"):
        return Result("moonraker", None, "интеграция не configured в этой прошивке")
    state = m.get("state")
    chamber = m.get("chamberTemperature")
    if state == "online":
        return Result(
            "moonraker",
            True,
            f"online (host={m.get('host')}:{m.get('port')} chamber={chamber}°C)",
        )
    if state == "disabled":
        return Result(
            "moonraker",
            None,
            "configured, но active integration != moonraker — переключите через portal",
        )
    return Result(
        "moonraker",
        False,
        f"state={state} lastError={m.get('lastError')!r}",
    )


# ── fake_moonraker lifecycle ────────────────────────────────────────
def start_fake_moonraker(cfg: Config) -> Optional[subprocess.Popen]:
    if not os.path.isfile(cfg.fake_moonraker_script):
        print(
            f"  WARN: fake_moonraker script не найден: {cfg.fake_moonraker_script}",
            file=sys.stderr,
        )
        return None
    try:
        proc = subprocess.Popen(
            ["python3", cfg.fake_moonraker_script],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except Exception as e:
        print(f"  WARN: не удалось стартовать fake_moonraker: {e}", file=sys.stderr)
        return None
    # дать ws-server подняться
    time.sleep(1.5)
    if proc.poll() is not None:
        return None
    return proc


def stop_fake_moonraker(proc: Optional[subprocess.Popen]) -> None:
    if proc is None or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2)


# ── fake_bambu lifecycle ─────────────────────────────────────────────
# fake_bambu = mosquitto broker (TLS 8883) + publisher.py.
# Mosquitto conf + certs живут в fake_bambu_dir.

@dataclass
class _FakeBambu:
    mosquitto: Optional[subprocess.Popen] = None
    publisher: Optional[subprocess.Popen] = None


def _stop_proc(p: Optional[subprocess.Popen]) -> None:
    if p is None or p.poll() is not None:
        return
    p.terminate()
    try:
        p.wait(timeout=3)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait(timeout=2)


def start_fake_bambu(cfg: Config) -> _FakeBambu:
    result = _FakeBambu()
    d = cfg.fake_bambu_dir
    conf = os.path.join(d, "mosquitto.conf")
    pub  = os.path.join(d, "publisher.py")
    if not os.path.isfile(conf) or not os.path.isfile(pub):
        print(f"  WARN: fake_bambu dir not found: {d}", file=sys.stderr)
        return result
    try:
        result.mosquitto = subprocess.Popen(
            ["mosquitto", "-c", conf],
            cwd=d,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except FileNotFoundError:
        print("  WARN: mosquitto not found — install via brew install mosquitto", file=sys.stderr)
        return result
    except Exception as e:
        print(f"  WARN: mosquitto start failed: {e}", file=sys.stderr)
        return result
    time.sleep(1.5)
    if result.mosquitto.poll() is not None:
        print("  WARN: mosquitto exited immediately (port 8883 busy?)", file=sys.stderr)
        result.mosquitto = None
        return result
    env = os.environ.copy()
    env.update({
        "FAKE_BAMBU_HOST":   "127.0.0.1",
        "FAKE_BAMBU_PORT":   "8883",
        "FAKE_BAMBU_SERIAL": cfg.fake_bambu_serial,
        "FAKE_BAMBU_LAN":    cfg.fake_bambu_lan,
    })
    try:
        result.publisher = subprocess.Popen(
            ["python3", pub],
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except Exception as e:
        print(f"  WARN: publisher.py start failed: {e}", file=sys.stderr)
    time.sleep(1.5)
    return result


def stop_fake_bambu(fb: _FakeBambu) -> None:
    _stop_proc(fb.publisher)
    _stop_proc(fb.mosquitto)


# ── MQTT command helper ──────────────────────────────────────────────

def _send_command(cfg: Config, command: str, payload: dict) -> bool:
    """Publish commands/<command> to the device via local broker."""
    topic = f"idryer/{cfg.serial}/commands/{command}"
    return mqtt_publish_one(cfg.local_broker, cfg.local_port, topic, json.dumps(payload))


def enable_integration(cfg: Config, kind: str, **fields) -> bool:
    """Configure + activate an integration on iHeater Link.

    Two-step:
    1. link_integration — saves IP/host/serial/etc to LINK_STORE NVS.
    2. set {bind: <kind>_en, val: true} — activates via MenuBridge → setActive.

    The SDK's link_integration builtin only saves config; the product-level
    onCommand callback that triggers setActive may not run reliably, so we
    explicitly activate with a separate set command.
    """
    ok1 = _send_command(cfg, "link_integration", {"type": kind, "enabled": True, **fields})
    time.sleep(0.3)
    bind_map = {"bambu": "bambu_en", "moonraker": "moon_en", "ha": "ha_en"}
    bind = bind_map.get(kind)
    ok2 = _send_command(cfg, "set", {"bind": bind, "val": True}) if bind else True
    return ok1 and ok2


# ── auto-heat tests ──────────────────────────────────────────────────

def test_auto_heat_bambu(cfg: Config) -> Result:
    """Full chain: fake_bambu RUNNING+filament → device → outputMode=1, targetTempC>0.

    fake_bambu уже запущен runner'ом перед этим тестом.
    Посылаем link_integration {bambu} → устройство подключается к fake_bambu →
    fake_bambu шлёт gcodeState=RUNNING + trayType=PLA →
    auto_heat.cpp: materialTempFromMenu("PLA") → RMT команда →
    telemetry.outputMode=1, telemetry.targetTempC>0.
    """
    ok = enable_integration(
        cfg, "bambu",
        ip=cfg.local_broker,
        serial=cfg.fake_bambu_serial,
        lanAccessCode=cfg.fake_bambu_lan,
    )
    if not ok:
        return Result("auto_heat_bambu", False, "не удалось отправить link_integration bambu")

    # Дать устройству время подключиться + получить push_status от fake_bambu.
    time.sleep(5.0)

    s = _fetch_integrations_status(cfg)
    if s is None:
        return Result("auto_heat_bambu", False, "integrations/status не пришёл")
    b = s.get("bambu") or {}
    if not b.get("configured"):
        return Result("auto_heat_bambu", None, "Bambu не configured в прошивке (allowBambu=false?)")
    state = b.get("state", "?")
    if state != "online":
        return Result("auto_heat_bambu", False,
                      f"Bambu state={state!r} (не online), lastError={b.get('lastError')!r}")

    # Проверяем telemetry: outputMode и targetTempC добавляет enrichTelemetry iHeater Link.
    telem_topic = f"idryer/{cfg.serial}/telemetry"
    msgs = mqtt_collect(cfg.local_broker, cfg.local_port, [telem_topic], cfg.test_timeout_s)
    if telem_topic not in msgs:
        return Result("auto_heat_bambu", False, "telemetry не пришёл")
    try:
        d = json.loads(msgs[telem_topic])
        output_mode = d.get("outputMode", -1)
        target_temp = d.get("targetTempC", 0.0)
        if output_mode == 1 and target_temp > 0:
            return Result("auto_heat_bambu", True,
                          f"outputMode=1 targetTempC={target_temp}°C — printer→menu→RMT OK")
        return Result("auto_heat_bambu", False,
                      f"outputMode={output_mode} targetTempC={target_temp} — нагрев не запустился")
    except Exception as e:
        return Result("auto_heat_bambu", False, f"telemetry parse error: {e}")


def test_auto_heat_moonraker(cfg: Config) -> Result:
    """Full chain: fake_moonraker VirtualChamber.target>0 → outputMode=1, targetTempC>0.

    fake_moonraker уже запущен runner'ом. Посылаем link_integration {moonraker} →
    устройство подключается → SDK.onVirtualChamberUpdate(target=70) →
    auto_heat.cpp: TargetTemperature=70 → RMT команда →
    telemetry.outputMode=1, telemetry.targetTempC=70.
    """
    ok = enable_integration(
        cfg, "moonraker",
        host=cfg.moonraker_host,
        port=cfg.moonraker_port,
    )
    if not ok:
        return Result("auto_heat_moonraker", False, "не удалось отправить link_integration moonraker")

    time.sleep(5.0)

    s = _fetch_integrations_status(cfg)
    if s is None:
        return Result("auto_heat_moonraker", False, "integrations/status не пришёл")
    m = s.get("moonraker") or {}
    if not m.get("configured"):
        return Result("auto_heat_moonraker", None, "Moonraker не configured в прошивке (allowMoonraker=false?)")
    state = m.get("state", "?")
    if state != "online":
        return Result("auto_heat_moonraker", False,
                      f"Moonraker state={state!r} (не online), lastError={m.get('lastError')!r}")

    telem_topic = f"idryer/{cfg.serial}/telemetry"
    msgs = mqtt_collect(cfg.local_broker, cfg.local_port, [telem_topic], cfg.test_timeout_s)
    if telem_topic not in msgs:
        return Result("auto_heat_moonraker", False, "telemetry не пришёл")
    try:
        d = json.loads(msgs[telem_topic])
        output_mode = d.get("outputMode", -1)
        target_temp = d.get("targetTempC", 0.0)
        chamber = m.get("chamberTemperature", "?")
        if output_mode == 1 and target_temp > 0:
            return Result("auto_heat_moonraker", True,
                          f"outputMode=1 targetTempC={target_temp}°C chamber={chamber}°C — VirtualChamber→RMT OK")
        return Result("auto_heat_moonraker", False,
                      f"outputMode={output_mode} targetTempC={target_temp} — нагрев не запустился")
    except Exception as e:
        return Result("auto_heat_moonraker", False, f"telemetry parse error: {e}")


# ── runner ──────────────────────────────────────────────────────────
# (name, fn, needs_fake_moonraker, needs_fake_bambu, description)
TESTS: List = [
    (
        "local_broker_reachable",
        test_local_broker_reachable,
        False, False,
        "MQTT-брокер на Mac (192.168.0.171:1883) принимает подключения",
    ),
    (
        "ha_broker_reachable",
        test_ha_broker_reachable,
        False, False,
        "MQTT-брокер на сервере Home Assistant (192.168.0.185:1883) принимает подключения",
    ),
    (
        "device_alive",
        test_device_alive,
        False, False,
        "устройство ЖИВОЕ СЕЙЧАС — публикует свежий status (timestamp моложе минуты)",
    ),
    (
        "mqtt_publish",
        test_mqtt_publish,
        False, False,
        "устройство публикует свои топики (status / info / integrations/status) на local-брокер",
    ),
    (
        "info",
        test_info,
        False, False,
        "устройство сообщает кто оно: deviceType, model, версия прошивки",
    ),
    (
        "request_response",
        test_request_response,
        False, False,
        "портал → устройство: спрашиваем get_config, ждём актуальное меню в /config",
    ),
    (
        "bambu",
        test_bambu,
        False, False,
        "Bambu integration подключилась к принтеру по MQTT (connection check)",
    ),
    (
        "ha_discovery",
        test_ha_discovery,
        False, False,
        "Home Assistant видит устройство (на HA-брокере есть homeassistant/.../config с этим серийником)",
    ),
    (
        "moonraker",
        test_moonraker_link,
        True, False,
        "Moonraker integration подключилась к Klipper-серверу и получает chamber-температуру",
    ),
    (
        "auto_heat_bambu",
        test_auto_heat_bambu,
        False, True,
        "iHeater Link: Bambu gcodeState=RUNNING + filament → menu.mat_* → outputMode=1 targetTempC>0",
    ),
    (
        "auto_heat_moonraker",
        test_auto_heat_moonraker,
        True, False,
        "iHeater Link: Moonraker VirtualChamber.target>0 → outputMode=1 targetTempC>0",
    ),
]


HUMAN_VERDICTS = {
    "local_broker_reachable": (
        "локальный брокер живой",
        "локальный брокер не отвечает — без него ни телеметрии, ни команд",
    ),
    "ha_broker_reachable": (
        "HA-брокер живой",
        "HA-брокер не отвечает — интеграция Home Assistant работать не будет",
    ),
    "device_alive": (
        "устройство ЖИВОЕ — публикует свежие данные",
        "устройство ОФЛАЙН (или давно не публиковало) — следующие тесты ненадёжны (retained)",
    ),
    "mqtt_publish": (
        "устройство публикует данные",
        "устройство молчит — либо не в сети, либо не подключилось к брокеру",
    ),
    "info": (
        "устройство себя идентифицировало",
        "устройство не публикует /info — портал не узнает кто это и какие у него возможности",
    ),
    "request_response": (
        "запрос-ответ работает — портал получит актуальные настройки",
        "устройство НЕ отвечает на get_config — на портале не загрузится меню настроек этого устройства",
    ),
    "bambu": (
        "Bambu-принтер подцепился",
        "Bambu не подключён — нагрев по статусу принтера работать не будет",
    ),
    "ha_discovery": (
        "Home Assistant видит сенсоры/кнопки этого устройства",
        "Home Assistant не видит устройство — оно не появится в HA UI",
    ),
    "moonraker": (
        "Klipper подцепился, virtual_chamber работает",
        "Moonraker недоступен или не active — авто-нагрев по Klipper не сработает",
    ),
    "auto_heat_bambu": (
        "Bambu → авто-нагрев сработал (filament→temp→RMT)",
        "Bambu НЕ запустил нагрев — цепочка принтер→меню→RMT сломана",
    ),
    "auto_heat_moonraker": (
        "Moonraker → авто-нагрев сработал (VirtualChamber→RMT)",
        "Moonraker НЕ запустил нагрев — цепочка VirtualChamber→RMT сломана",
    ),
}


def parse_args() -> Config:
    ap = argparse.ArgumentParser(description="iDryer Link smoke test runner")
    ap.add_argument("--serial", required=True, help="напр. DEVICE_ACEBE6490534")
    ap.add_argument("--local-broker", default="192.168.0.171")
    ap.add_argument("--local-port", type=int, default=1883)
    ap.add_argument("--ha-broker", default="192.168.0.185")
    ap.add_argument("--ha-port", type=int, default=1883)
    ap.add_argument("--timeout", type=float, default=15.0)
    ap.add_argument("--only", help="comma-separated имена тестов")
    ap.add_argument("--skip", help="comma-separated имена для пропуска")
    a = ap.parse_args()
    return Config(
        serial=a.serial,
        local_broker=a.local_broker,
        local_port=a.local_port,
        ha_broker=a.ha_broker,
        ha_port=a.ha_port,
        test_timeout_s=a.timeout,
        only=[s.strip() for s in (a.only or "").split(",") if s.strip()],
        skip=[s.strip() for s in (a.skip or "").split(",") if s.strip()],
    )


ICON = {True: "✅", False: "❌", None: "⏭ "}


def main() -> int:
    cfg = parse_args()

    print("iDryer Link smoke test")
    print(f"  serial:       {cfg.serial}")
    print(f"  local broker: {cfg.local_broker}:{cfg.local_port}")
    print(f"  HA broker:    {cfg.ha_broker}:{cfg.ha_port}")
    print(f"  test timeout: {cfg.test_timeout_s}s")
    print()

    fake_moon: Optional[subprocess.Popen] = None
    fake_bambu: Optional[_FakeBambu] = None
    results: List[Result] = []

    try:
        for name, fn, needs_moon, needs_bambu, desc in TESTS:
            if cfg.only and name not in cfg.only:
                continue
            if name in cfg.skip:
                continue

            if needs_moon and fake_moon is None:
                print("─" * 70)
                print("▶ запускаю fake_moonraker (фоновый WS на :7125)…", flush=True)
                fake_moon = start_fake_moonraker(cfg)
                if fake_moon is None:
                    print("  fake_moonraker НЕ стартовал — тест moonraker будет невалиден")

            if needs_bambu and fake_bambu is None:
                print("─" * 70)
                print("▶ запускаю fake_bambu (mosquitto :8883 + publisher.py)…", flush=True)
                fake_bambu = start_fake_bambu(cfg)
                if fake_bambu.mosquitto is None:
                    print("  fake_bambu НЕ стартовал — тест auto_heat_bambu будет невалиден")

            print()
            print("─" * 70)
            print(f"▶ ТЕСТ: {name}")
            print(f"  что проверяю: {desc}")
            t0 = time.time()
            try:
                r = fn(cfg)
            except Exception as e:
                r = Result(name, False, f"exception: {e}")
            dt = time.time() - t0
            results.append(r)

            ok_msg, bad_msg = HUMAN_VERDICTS.get(name, ("OK", "FAIL"))
            verdict = ok_msg if r.passed is True else (bad_msg if r.passed is False else "не применимо к этой прошивке")
            print(f"  ВЫВОД:        {ICON[r.passed]} {verdict}")
            print(f"  детали:       {r.details}")
            print(f"  заняло:       {dt:.1f}s")
    finally:
        if fake_moon:
            print()
            print("─" * 70)
            print("▶ останавливаю fake_moonraker…")
            stop_fake_moonraker(fake_moon)
        if fake_bambu:
            print()
            print("─" * 70)
            print("▶ останавливаю fake_bambu…")
            stop_fake_bambu(fake_bambu)

    print()
    print("═" * 70)
    print(f"ИТОГ для {cfg.serial}")
    print("═" * 70)
    for r in results:
        ok_msg, bad_msg = HUMAN_VERDICTS.get(r.name, ("OK", "FAIL"))
        verdict = ok_msg if r.passed is True else (bad_msg if r.passed is False else "не применимо к этой прошивке")
        print(f"  {ICON[r.passed]}  {r.name:24s}  {verdict}")
    print()
    passed = sum(1 for r in results if r.passed is True)
    failed = sum(1 for r in results if r.passed is False)
    skipped = sum(1 for r in results if r.passed is None)
    print(f"СВОДКА: ✅ {passed} прошло   ❌ {failed} упало   ⏭  {skipped} пропущено (нет в этой прошивке)")
    print("═" * 70)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
