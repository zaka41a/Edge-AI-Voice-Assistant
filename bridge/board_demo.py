#!/usr/bin/env python3
"""Drive the board over the framed USB protocol and watch it react.

Flash firmware/ (edge_ai_firmware) first, then run:

    ./.venv/bin/python board_demo.py                 # auto-detect the port
    ./.venv/bin/python board_demo.py /dev/cu.usbmodemXXXX

It scripts a full interaction (listening -> thinking -> speaking with several
moods, plus a "set LEDs red" command) so the LCD face and the WS2812 LEDs go
through their whole repertoire. ACK/LOG frames coming back are printed.
"""
from __future__ import annotations

import glob
import sys
import time

import serial  # pyserial

import protocol as p


def find_port(arg: str | None) -> str:
    if arg:
        return arg
    cands = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    if not cands:
        sys.exit("No serial port found. Pass it explicitly: board_demo.py /dev/cu.usbmodemXXXX")
    return cands[0]


def main() -> None:
    port = find_port(sys.argv[1] if len(sys.argv) > 1 else None)
    print(f"Opening {port} ...")
    ser = serial.Serial(port, 115200, timeout=0.1)
    time.sleep(0.3)

    dec = p.Decoder()

    def send(frame: bytes, note: str, repeat: int = 3) -> None:
        # The firmware is RX-only and the commands are idempotent, so we send
        # each one a few times: cheap insurance against this debug-probe UART
        # bridge dropping bytes. Any ACK/LOG the board does emit is printed.
        print(f"  -> {note}")
        for _ in range(repeat):
            ser.write(frame)
            time.sleep(0.04)
        for f in dec.feed(ser.read(128)):
            if f.type == p.Type.ACK:
                print(f"     <- ACK seq={f.seq}")
            elif f.type == p.Type.LOG:
                print(f"     <- LOG '{f.payload.decode(errors='replace')}'")

    # A scripted conversation: state changes + moods + a direct LED command.
    # Each frame gets a distinct sequence number so the ACKs are traceable.
    steps = [
        (lambda q: p.state_frame("idle", q),        "state = IDLE (breathing)"),
        (lambda q: p.state_frame("listening", q),   "state = LISTENING (blue scan)"),
        (lambda q: p.state_frame("thinking", q),    "state = THINKING (orange spinner)"),
        (lambda q: p.mood_frame("happy", q),        "mood = HAPPY (green)"),
        (lambda q: p.state_frame("speaking", q),    "state = SPEAKING (pulse)"),
        (lambda q: p.mood_frame("curious", q),      "mood = CURIOUS (gold)"),
        (lambda q: p.mood_frame("sad", q),          "mood = SAD (blue)"),
        (lambda q: p.mood_frame("angry", q),        "mood = ANGRY (red)"),
        (lambda q: p.led_rgb_frame(255, 0, 0, q),   "CMD set LEDs solid RED (function-calling)"),
        (lambda q: p.led_rgb_frame(0, 0, 255, q),   "CMD set LEDs solid BLUE"),
        (lambda q: p.state_frame("idle", q),        "state = IDLE (back to rest)"),
        (lambda q: p.mood_frame("neutral", q),      "mood = NEUTRAL (cyan)"),
    ]

    print("Driving the board (watch the LCD face + the 8 RGB LEDs):\n")
    for seq, (build, note) in enumerate(steps):
        send(build(seq & 0xFF), note)
        time.sleep(1.5)

    print("\nDone. The board keeps animating on its own.")
    ser.close()


if __name__ == "__main__":
    main()
