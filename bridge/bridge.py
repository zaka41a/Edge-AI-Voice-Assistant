#!/usr/bin/env python3
"""Edge AI Voice Assistant - host bridge (skeleton).

MILESTONE 1: prove the USB link to the RP2350 and the PostgreSQL
history logging. This is the foundation; STT / LLM / TTS get plugged
into `handle_line()` later.

Usage:
    python bridge.py --port /dev/tty.usbmodemXXXX
    python bridge.py --list                 # list available serial ports
    python bridge.py --history              # print the last interactions
    python bridge.py --no-db                # skip database logging

Type a line and press Enter; it is sent to the board, which echoes it
back. Each exchange is logged to PostgreSQL.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time

import serial
import serial.tools.list_ports

from db import Database

DEFAULT_CONFIG = {
    "serial": {"port": "/dev/tty.usbmodem0001", "baudrate": 115200},
    "database": {"dsn": "postgresql://edgeai:edgeai@localhost:5544/edgeai"},
}


def load_config(path: str) -> dict:
    cfg = json.loads(json.dumps(DEFAULT_CONFIG))  # deep copy of defaults
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            user = json.load(f)
        for section, values in user.items():
            cfg.setdefault(section, {}).update(values)
    return cfg


def list_ports() -> None:
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for p in ports:
        print(f"  {p.device:<24} {p.description}")


def print_history(db: Database, n: int) -> None:
    rows = db.recent(n)
    if not rows:
        print("(history is empty)")
        return
    print(f"Last {len(rows)} interactions:")
    for created_at, mode, question, reply, mood, latency in rows:
        ts = created_at.strftime("%Y-%m-%d %H:%M:%S")
        print(f"  [{ts}] ({mode}) Q: {question!r} -> R: {reply!r} "
              f"mood={mood} {latency}ms")


def handle_line(ser: serial.Serial, line: str) -> str:
    """Send a line to the board and return whatever it echoes back.

    Later this is where STT output goes in and TTS/LCD/LED commands
    come out. For now it is a plain echo round-trip.
    """
    ser.reset_input_buffer()
    ser.write((line + "\n").encode("utf-8"))
    time.sleep(0.1)
    return ser.read(4096).decode("utf-8", errors="replace").strip()


def main() -> int:
    ap = argparse.ArgumentParser(description="Edge AI Voice Assistant bridge")
    ap.add_argument("--config", default="config.json")
    ap.add_argument("--port", help="serial port (overrides config)")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    ap.add_argument("--history", action="store_true", help="print recent history and exit")
    ap.add_argument("--no-db", action="store_true", help="disable database logging")
    args = ap.parse_args()

    if args.list:
        list_ports()
        return 0

    cfg = load_config(args.config)
    dsn = cfg["database"]["dsn"]

    db = None
    if not args.no_db:
        try:
            db = Database(dsn)
        except Exception as exc:  # noqa: BLE001
            print(f"WARNING: could not connect to PostgreSQL ({exc}).")
            print("         Start it with:  cd ../db && docker compose up -d")
            print("         Continuing without history logging (--no-db).")

    if args.history:
        if db is None:
            print("No database connection.")
            return 1
        print_history(db, 20)
        return 0

    port = args.port or cfg["serial"]["port"]
    baud = cfg["serial"]["baudrate"]
    try:
        ser = serial.Serial(port, baud, timeout=0.2)
    except serial.SerialException as exc:
        print(f"ERROR: cannot open serial port {port}: {exc}")
        print("Tip: run  python bridge.py --list  to see available ports.")
        return 1

    print(f"Connected to {port} @ {baud} baud.")
    print("Type a line and press Enter to send it to the board (Ctrl-C to quit).")
    try:
        while True:
            line = input("> ")
            if not line:
                continue
            t0 = time.time()
            reply = handle_line(ser, line)
            latency_ms = int((time.time() - t0) * 1000)
            print(f"board: {reply}")
            if db is not None:
                db.log(mode="echo-test", question=line, reply=reply,
                       mood=None, sensors={}, latency_ms=latency_ms)
    except KeyboardInterrupt:
        print("\nBye.")
    finally:
        ser.close()
        if db is not None:
            db.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
