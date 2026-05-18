#!/usr/bin/env python3
"""
device_smoke_test.py — универсальный smoke-тест для любого idryer-core устройства.

Проверяет по MQTT:
  1. info       — устройство публикует idryer/<serial>/info
  2. telemetry  — устройство публикует idryer/<serial>/telemetry
  3. get_config — команда get_config → ответ idryer/<serial>/config

Пример:
  python3 device_smoke_test.py --serial DEVICE_ACEBE648EAD0 --broker 192.168.1.27
  python3 device_smoke_test.py --serial DEVICE_ACEBE648EAD0 --broker 192.168.1.27 --timeout 15
"""

import argparse
import json
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("FATAL: pip3 install paho-mqtt", file=sys.stderr)
    sys.exit(2)


def _new_client(client_id=""):
    try:
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    except AttributeError:
        return mqtt.Client(client_id=client_id)


def _collect(broker, port, topics, timeout):
    """Subscribe and return {topic: payload_str} for the first message on each topic."""
    received = {}
    done = threading.Event() if False else None  # не используем threading — простой poll

    client = _new_client("smoke_collect")
    client.connect(broker, port, keepalive=10)

    def on_message(c, u, msg):
        t = msg.topic
        if t not in received:
            received[t] = msg.payload.decode("utf-8", errors="replace")

    client.on_message = on_message
    for t in topics:
        client.subscribe(t)

    client.loop_start()
    deadline = time.time() + timeout
    while time.time() < deadline:
        if all(t in received for t in topics):
            break
        time.sleep(0.2)
    client.loop_stop()
    client.disconnect()
    return received


def _publish(broker, port, topic, payload):
    client = _new_client("smoke_pub")
    client.connect(broker, port, keepalive=10)
    client.loop_start()
    info = client.publish(topic, payload, qos=0)
    info.wait_for_publish(timeout=5)
    client.loop_stop()
    client.disconnect()
    return info.rc == mqtt.MQTT_ERR_SUCCESS


# ── test functions ───────────────────────────────────────────────────


def test_info(serial, broker, port, timeout):
    topic = f"idryer/{serial}/info"
    msgs = _collect(broker, port, [topic], timeout)
    if topic not in msgs:
        return False, "нет сообщения"
    try:
        d = json.loads(msgs[topic])
        fw = d.get("firmware", d.get("fw", "?"))
        return True, f"fw={fw}"
    except Exception:
        return True, "получено (не JSON)"


def test_telemetry(serial, broker, port, timeout):
    topic = f"idryer/{serial}/telemetry"
    msgs = _collect(broker, port, [topic], timeout)
    if topic not in msgs:
        return False, "нет сообщения"
    try:
        d = json.loads(msgs[topic])
        keys = list(d.keys())[:3]
        return True, f"keys={keys}"
    except Exception:
        return True, "получено (не JSON)"


def test_get_config(serial, broker, port, timeout):
    cmd_topic = f"idryer/{serial}/commands/get_config"
    cfg_topic = f"idryer/{serial}/config"

    # Subscribe first, then publish
    received = {}
    client = _new_client("smoke_cfg")
    client.connect(broker, port, keepalive=10)

    def on_message(c, u, msg):
        if msg.topic not in received:
            received[msg.topic] = msg.payload.decode("utf-8", errors="replace")

    client.on_message = on_message
    client.subscribe(cfg_topic)
    client.loop_start()
    time.sleep(0.3)

    pub_ts = time.time()
    client.publish(cmd_topic, "{}", qos=0)

    deadline = time.time() + timeout
    while time.time() < deadline:
        if cfg_topic in received and time.time() >= pub_ts:
            break
        time.sleep(0.2)
    client.loop_stop()
    client.disconnect()

    if cfg_topic not in received:
        return False, "нет ответа на get_config"
    elapsed = time.time() - pub_ts
    try:
        d = json.loads(received[cfg_topic])
        top_keys = list(d.keys())[:2]
        return True, f"keys={top_keys} ({elapsed:.1f}s)"
    except Exception:
        return True, f"получено ({elapsed:.1f}s)"


# ── runner ───────────────────────────────────────────────────────────

TESTS = [
    ("info",       test_info),
    ("telemetry",  test_telemetry),
    ("get_config", test_get_config),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial",  required=True, help="Device serial, e.g. DEVICE_ACEBE648EAD0")
    ap.add_argument("--broker",  default="192.168.1.27")
    ap.add_argument("--port",    type=int, default=1883)
    ap.add_argument("--timeout", type=float, default=10.0, help="Seconds per test")
    ap.add_argument("--only",    default="", help="Comma-separated test names to run")
    args = ap.parse_args()

    only = set(args.only.split(",")) if args.only else set()
    tests = [(n, fn) for n, fn in TESTS if not only or n in only]

    total = len(tests)
    passed = 0

    print(f"\ndevice: {args.serial}  broker: {args.broker}:{args.port}\n")

    for i, (name, fn) in enumerate(tests, 1):
        label = f"[{i}/{total}] {name}"
        dots = "." * max(1, 28 - len(label))
        sys.stdout.write(f"{label} {dots} ")
        sys.stdout.flush()

        t0 = time.time()
        try:
            ok, detail = fn(args.serial, args.broker, args.port, args.timeout)
        except Exception as e:
            ok, detail = False, str(e)
        elapsed = time.time() - t0

        status = "OK " if ok else "FAIL"
        print(f"{status}  {detail}  ({elapsed:.1f}s)")
        if ok:
            passed += 1

    print(f"\n{'─' * 40}")
    if passed == total:
        print(f"PASS {passed}/{total}")
    else:
        print(f"FAIL {passed}/{total}")
    print()
    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()
