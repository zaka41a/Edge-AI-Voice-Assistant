from __future__ import annotations

import glob
import os
import threading
import time

import protocol as p

try:
    import serial
except ImportError:
    serial = None

def _autodetect() -> str | None:
    env = os.environ.get("EDGEAI_BOARD_PORT")
    if env:
        return env
    cands = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    return cands[0] if cands else None

class BoardLink:
    def __init__(self, port: str | None = None, baud: int = 115200, repeat: int = 3):
        self.repeat = repeat
        self._seq = 0
        self._ser = None
        self.port = port or _autodetect()
        if serial is not None and self.port:
            try:
                self._ser = serial.Serial(self.port, baud, timeout=0.1)
                time.sleep(0.2)
            except Exception:
                self._ser = None

    @property
    def connected(self) -> bool:
        return self._ser is not None

    def _send(self, frame: bytes) -> None:
        if self._ser is None:
            return
        try:
            for _ in range(self.repeat):
                self._ser.write(frame)
                time.sleep(0.03)
        except Exception:
            self._ser = None

    def _next_seq(self) -> int:
        self._seq = (self._seq + 1) & 0xFF
        return self._seq

    def set_state(self, state: str) -> None:
        self._send(p.state_frame(state, self._next_seq()))

    def set_mood(self, mood: str) -> None:
        self._send(p.mood_frame(mood, self._next_seq()))

    def send_text(self, text: str) -> None:
        self._send(p.text_frame(text, self._next_seq()))

    def set_led_rgb(self, r: int, g: int, b: int) -> None:
        self._send(p.led_rgb_frame(r, g, b, self._next_seq()))

    def play_audio(self, pcm8: bytes, chunk: int = 480) -> None:

        if self._ser is None or not pcm8:
            return
        try:
            seq = 0
            for i in range(0, len(pcm8), chunk):
                self._ser.write(p.encode(p.Type.AUDIO_OUT, seq & 0xFF, pcm8[i:i + chunk]))
                seq += 1
                time.sleep(0.002)
            self._ser.write(p.encode(p.Type.AUDIO_OUT, seq & 0xFF, b""))
        except Exception:
            self._ser = None

    def start_reader(self, on_audio, sample_rate: int = 16000) -> None:

        if self._ser is None:
            return
        self._on_audio = on_audio
        self._rate = sample_rate
        threading.Thread(target=self._read_loop, daemon=True).start()

    def _read_loop(self) -> None:
        dec = p.Decoder()
        audio = bytearray()
        receiving = False
        frames = 0
        dropped = 0
        prev_seq = None
        while self._ser is not None:
            try:
                data = self._ser.read(1024)
            except Exception:
                break
            if not data:
                continue
            for f in dec.feed(data):
                if f.type != p.Type.AUDIO_IN:
                    continue
                if len(f.payload) == 0:
                    if receiving and audio and self._on_audio:
                        stats = {"frames": frames, "dropped": dropped,
                                 "decoder_dropped": dec.dropped}
                        self._on_audio(bytes(audio), self._rate, stats)
                    audio = bytearray()
                    receiving = False
                    frames = 0
                    dropped = 0
                    prev_seq = None
                else:
                    if prev_seq is not None:
                        dropped += (f.seq - prev_seq - 1) & 0xFF
                    prev_seq = f.seq
                    frames += 1
                    audio += f.payload
                    receiving = True

    def close(self) -> None:
        if self._ser is not None:
            try:
                self._ser.close()
            finally:
                self._ser = None
