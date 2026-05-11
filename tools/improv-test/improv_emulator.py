#!/usr/bin/env python3
"""
Improv-WiFi portal emulator over USB CDC.
Drives the same RPC flow as install.idryer.org but from CLI.

Protocol: 'I','M','P','R','O','V', version=1, type, len, [data], checksum.
Types: 1=CURRENT_STATE, 2=ERROR_STATE, 3=RPC, 4=RPC_RESPONSE.
RPC commands: 1=WIFI_SETTINGS, 2=IDENTIFY/GET_CURRENT_STATE, 3=GET_DEVICE_INFO, 4=GET_WIFI_NETWORKS.
States: 1=AUTHORIZATION_REQUIRED, 2=AUTHORIZED, 3=PROVISIONING, 4=PROVISIONED.
"""
import argparse
import sys
import time
import serial

MAGIC = b'IMPROV'
VERSION = 1
TYPE_CURRENT_STATE = 1
TYPE_ERROR_STATE = 2
TYPE_RPC = 3
TYPE_RPC_RESPONSE = 4
CMD_WIFI_SETTINGS = 1
CMD_GET_DEVICE_INFO = 3
STATE_AUTHORIZED = 2
STATE_PROVISIONING = 3
STATE_PROVISIONED = 4


def build_frame(frame_type: int, payload: bytes) -> bytes:
    body = MAGIC + bytes([VERSION, frame_type, len(payload)]) + payload
    checksum = sum(body) & 0xFF
    return body + bytes([checksum])


def build_wifi_settings_rpc(ssid: str, password: str) -> bytes:
    ssid_b = ssid.encode('utf-8')
    pass_b = password.encode('utf-8')
    data = bytes([len(ssid_b)]) + ssid_b + bytes([len(pass_b)]) + pass_b
    rpc_body = bytes([CMD_WIFI_SETTINGS, len(data)]) + data
    return build_frame(TYPE_RPC, rpc_body)


def parse_frames(buf: bytearray):
    """Yield (type, payload) tuples for any complete IMPROV frames in buf."""
    while True:
        idx = buf.find(MAGIC)
        if idx < 0:
            buf.clear()
            return
        if idx > 0:
            del buf[:idx]
        if len(buf) < 11:
            return
        version = buf[6]
        ftype = buf[7]
        flen = buf[8]
        total = 9 + flen + 1
        if len(buf) < total:
            return
        payload = bytes(buf[9:9 + flen])
        checksum_received = buf[9 + flen]
        checksum_calc = sum(buf[:9 + flen]) & 0xFF
        del buf[:total]
        yield ftype, payload, (checksum_received == checksum_calc)


def describe(ftype, payload):
    if ftype == TYPE_CURRENT_STATE and len(payload) >= 1:
        state = payload[0]
        name = {1: "AUTHORIZATION_REQUIRED", 2: "AUTHORIZED",
                3: "PROVISIONING", 4: "PROVISIONED"}.get(state, f"UNKNOWN({state})")
        return f"STATE={name}"
    if ftype == TYPE_ERROR_STATE and len(payload) >= 1:
        return f"ERROR={payload[0]}"
    if ftype == TYPE_RPC_RESPONSE:
        return f"RPC_RESP cmd={payload[0]} data={payload[2:].decode('utf-8', errors='replace')!r}"
    return f"type={ftype} payload={payload.hex()}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem11401")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--ssid", required=True)
    ap.add_argument("--password", required=True)
    ap.add_argument("--timeout", type=float, default=60.0,
                    help="Seconds to wait for PROVISIONED state")
    args = ap.parse_args()

    print(f"[emulator] Opening {args.port} @ {args.baud}", flush=True)
    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    time.sleep(0.5)
    ser.reset_input_buffer()

    print(f"[emulator] Sending WIFI_SETTINGS ssid={args.ssid!r}", flush=True)
    frame = build_wifi_settings_rpc(args.ssid, args.password)
    ser.write(frame)
    ser.flush()

    deadline = time.time() + args.timeout
    buf = bytearray()
    provisioned = False
    last_state = None
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            for ftype, payload, ok in parse_frames(buf):
                msg = describe(ftype, payload)
                tag = "OK " if ok else "BAD"
                print(f"[emulator] {tag} <- {msg}", flush=True)
                if ftype == TYPE_CURRENT_STATE and len(payload) >= 1:
                    last_state = payload[0]
                    if payload[0] == STATE_PROVISIONED:
                        provisioned = True
                if ftype == TYPE_ERROR_STATE and len(payload) >= 1 and payload[0] != 0:
                    print(f"[emulator] FAIL error code {payload[0]}", flush=True)
                    ser.close()
                    return 2
        if provisioned:
            time.sleep(0.5)
            tail = ser.read(512)
            if tail:
                buf.extend(tail)
                for ftype, payload, ok in parse_frames(buf):
                    print(f"[emulator] tail <- {describe(ftype, payload)}", flush=True)
            ser.close()
            print("[emulator] SUCCESS: device reported PROVISIONED", flush=True)
            return 0

    ser.close()
    print(f"[emulator] TIMEOUT after {args.timeout}s; last state={last_state}", flush=True)
    return 1


if __name__ == "__main__":
    sys.exit(main())
