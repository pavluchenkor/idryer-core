#!/usr/bin/env python3
"""
local_auto_claim.py — claim device on local backend via serial.

Flow:
  1. Open serial port
  2. Wait for WiFi-ready marker, send START_CLAIM
  3. Wait for CLAIM_PIN:<pin>:<expires>
  4. POST /auth/login → POST /devices/claim

Credentials are read from .env in the current directory (or via args).
.env format:
  LOCAL_EMAIL=you@example.com
  LOCAL_PASSWORD=yourpassword
  LOCAL_API_URL=http://192.168.1.27:3000

Usage:
  python3 local_auto_claim.py --port /dev/cu.usbmodem1143401
  python3 local_auto_claim.py --port /dev/cu.usbmodem1143401 --api http://192.168.1.27:3000
"""

import argparse
import json
import os
import sys
import time
import urllib.request
import urllib.error

try:
    import serial as pyserial
except ImportError:
    print("FATAL: pip install pyserial", file=sys.stderr)
    sys.exit(2)

GREEN  = '\033[92m'
YELLOW = '\033[93m'
CYAN   = '\033[96m'
RED    = '\033[91m'
DIM    = '\033[2m'
RESET  = '\033[0m'

SERIAL_TIMEOUT = 120


def _load_dotenv():
    dotenv_path = os.path.join(os.getcwd(), ".env")
    if not os.path.isfile(dotenv_path):
        return
    with open(dotenv_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, val = line.partition("=")
            key = key.strip()
            val = val.strip().strip('"').strip("'")
            if key and key not in os.environ:
                os.environ[key] = val


def _api(base_url, path, data=None, token=None):
    url = f"{base_url}{path}"
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    body = json.dumps(data).encode() if data else None
    req = urllib.request.Request(url, data=body, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError as e:
        body = e.read().decode() if e.fp else ""
        print(f"  {RED}HTTP {e.code}: {body}{RESET}")
        return None
    except Exception as e:
        print(f"  {RED}Request failed: {e}{RESET}")
        return None


def main():
    _load_dotenv()

    ap = argparse.ArgumentParser()
    ap.add_argument("--port",     required=True, help="Serial port, e.g. /dev/cu.usbmodem1143401")
    ap.add_argument("--baud",     type=int, default=115200)
    ap.add_argument("--api",      default=os.getenv("LOCAL_API_URL", ""), help="Backend URL")
    ap.add_argument("--email",    default=os.getenv("LOCAL_EMAIL", ""))
    ap.add_argument("--password", default=os.getenv("LOCAL_PASSWORD", ""))
    ap.add_argument("--timeout",  type=int, default=SERIAL_TIMEOUT)
    args = ap.parse_args()

    if not args.email or not args.password or not args.api:
        missing = [k for k, v in [("--email/LOCAL_EMAIL", args.email),
                                   ("--password/LOCAL_PASSWORD", args.password),
                                   ("--api/LOCAL_API_URL", args.api)] if not v]
        print(f"{RED}Missing: {', '.join(missing)}{RESET}")
        print(f"{YELLOW}Set in .env or pass as args{RESET}")
        sys.exit(1)

    print()
    print(f"  {CYAN}{'=' * 52}{RESET}")
    print(f"  {CYAN}  LOCAL AUTO-CLAIM{RESET}")
    print(f"  {CYAN}  Port: {args.port} @ {args.baud}{RESET}")
    print(f"  {CYAN}  API:  {args.api}{RESET}")
    print(f"  {CYAN}  Waiting for CLAIM_PIN... (timeout: {args.timeout}s){RESET}")
    print(f"  {CYAN}{'=' * 52}{RESET}")
    print()

    pin = None
    claim_sent = False

    try:
        time.sleep(1)
        ser = pyserial.Serial(args.port, args.baud, timeout=1)
        start = time.time()

        while time.time() - start < args.timeout:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                except Exception:
                    continue
                if not line:
                    continue
                print(f"  {DIM}[SERIAL] {line}{RESET}")

                if not claim_sent and (
                    "WiFi ok" in line or
                    "NOT claimed" in line or
                    "provision OK" in line or
                    "Logs enabled after WiFi config" in line
                ):
                    time.sleep(1.5)
                    ser.write(b"START_CLAIM\n")
                    ser.flush()
                    claim_sent = True
                    print(f"\n  {CYAN}[CLAIM] START_CLAIM sent{RESET}")

                if line.startswith("CLAIM_PIN:"):
                    parts = line.split(":")
                    if len(parts) >= 2:
                        pin = parts[1]
                        expires = parts[2] if len(parts) >= 3 else "?"
                        print(f"\n  {GREEN}[CLAIM] PIN: {pin} (expires: {expires}s){RESET}")
                        break

                if line.startswith("CLAIM_ALREADY:"):
                    print(f"\n  {GREEN}[CLAIM] Device already claimed{RESET}")
                    ser.close()
                    return

        ser.close()

    except KeyboardInterrupt:
        print(f"\n  {YELLOW}[CLAIM] Skipped by user{RESET}")
        return
    except Exception as e:
        print(f"  {RED}[CLAIM] Serial error: {e}{RESET}")
        sys.exit(1)

    if not pin:
        print(f"  {YELLOW}[CLAIM] No PIN received within {args.timeout}s{RESET}")
        print(f"  {YELLOW}  - Check WiFi (run improv_emulator.py first){RESET}")
        print(f"  {YELLOW}  - Check backend at {args.api}{RESET}")
        sys.exit(1)

    print(f"\n  {CYAN}[CLAIM] Logging in...{RESET}")
    resp = _api(args.api, "/auth/login", {"email": args.email, "password": args.password})
    if not resp or "accessToken" not in resp:
        print(f"  {RED}[CLAIM] Login failed! PIN was: {pin}{RESET}")
        sys.exit(1)

    jwt = resp["accessToken"]
    print(f"  {GREEN}[CLAIM] Login OK{RESET}")

    print(f"  {CYAN}[CLAIM] Claiming with PIN {pin}...{RESET}")
    result = _api(args.api, "/devices/claim", {"pin": pin, "name": "iDryer Link"}, token=jwt)
    if result and result.get("claimed"):
        print(f"\n  {GREEN}{'=' * 52}{RESET}")
        print(f"  {GREEN}  CLAIMED OK: {result.get('deviceId')}{RESET}")
        print(f"  {GREEN}{'=' * 52}{RESET}")
    else:
        print(f"  {RED}[CLAIM] Claim failed! PIN: {pin}{RESET}")
        sys.exit(1)


if __name__ == "__main__":
    main()
