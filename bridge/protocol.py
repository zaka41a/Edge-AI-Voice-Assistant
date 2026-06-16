"""Framed USB protocol for the Edge AI Voice Assistant (host side).

Byte-exact mirror of firmware/src/protocol.h. Every frame is CRC-checked so the
receiver can resynchronise after lost or noisy bytes.

Frame layout:
    SYNC0 SYNC1 | TYPE | LEN_lo LEN_hi | SEQ | PAYLOAD[LEN] | CRC_lo CRC_hi
      0xAA 0x55 | 1 B  |    2 bytes    | 1 B |   N bytes    |   CRC16-CCITT

CRC16-CCITT (poly 0x1021, init 0xFFFF) covers TYPE, LEN, SEQ and PAYLOAD
(the two SYNC bytes are excluded), little-endian on the wire.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterator

SYNC0 = 0xAA
SYNC1 = 0x55
MAX_PAYLOAD = 1024
OVERHEAD = 8  # 2 sync + type + 2 len + seq + 2 crc


class Type:
    """Frame type byte. Keep in sync with proto::Type in protocol.h."""
    AUDIO_IN = 0x01
    AUDIO_OUT = 0x02
    TEXT = 0x03
    MOOD = 0x04
    SENSOR_REQ = 0x05
    SENSOR_RSP = 0x06
    CMD = 0x07
    ACK = 0x08
    LOG = 0x09


# MOOD payload byte values (matches Face::Mood / proto::Mood order).
MOOD_BYTE = {"neutral": 0, "happy": 1, "sad": 2, "angry": 3, "curious": 4}
MOOD_NAME = {v: k for k, v in MOOD_BYTE.items()}

# Assistant state byte (CMD_STATE payload[1]); matches Face::State / proto::DState.
STATE_BYTE = {"idle": 0, "listening": 1, "thinking": 2, "speaking": 3}


class Cmd:
    """CMD sub-command in payload[0]. Keep in sync with proto::Cmd."""
    STATE = 0x01    # payload[1] = state byte
    LED_RGB = 0x02  # payload[1..3] = R, G, B


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    """CRC16-CCITT, MSB-first, poly 0x1021, init 0xFFFF."""
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc & 0xFFFF


def encode(type_: int, seq: int, payload: bytes = b"") -> bytes:
    """Build a complete frame ready to write to the serial link."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too big: {len(payload)} > {MAX_PAYLOAD}")
    n = len(payload)
    body = bytes((type_ & 0xFF, n & 0xFF, (n >> 8) & 0xFF, seq & 0xFF)) + payload
    c = crc16(body)
    return bytes((SYNC0, SYNC1)) + body + bytes((c & 0xFF, (c >> 8) & 0xFF))


@dataclass
class Frame:
    type: int
    seq: int
    payload: bytes


class Decoder:
    """Streaming, resynchronising decoder.

    Feed it raw bytes; it yields every complete, CRC-valid frame it finds and
    silently drops garbage / corrupted frames (counted in `dropped`).
    """

    _S_SYNC0, _S_SYNC1, _S_TYPE, _S_LEN_LO, _S_LEN_HI, _S_SEQ, \
        _S_PAY, _S_CRC_LO, _S_CRC_HI = range(9)

    def __init__(self) -> None:
        self._st = self._S_SYNC0
        self._type = 0
        self._seq = 0
        self._len = 0
        self._crc_rx = 0
        self._buf = bytearray()
        self.dropped = 0

    def feed(self, data: bytes) -> Iterator[Frame]:
        for b in data:
            frame = self._step(b)
            if frame is not None:
                yield frame

    def _step(self, b: int) -> Frame | None:
        st = self._st
        if st == self._S_SYNC0:
            if b == SYNC0:
                self._st = self._S_SYNC1
            else:
                self.dropped += 1
        elif st == self._S_SYNC1:
            if b == SYNC1:
                self._st = self._S_TYPE
            elif b == SYNC0:
                pass  # stay: another possible start
            else:
                self.dropped += 1
                self._st = self._S_SYNC0
        elif st == self._S_TYPE:
            self._type = b
            self._st = self._S_LEN_LO
        elif st == self._S_LEN_LO:
            self._len = b
            self._st = self._S_LEN_HI
        elif st == self._S_LEN_HI:
            self._len |= b << 8
            if self._len > MAX_PAYLOAD:
                self.dropped += 1
                self._st = self._S_SYNC0
            else:
                self._buf = bytearray()
                self._st = self._S_SEQ
        elif st == self._S_SEQ:
            self._seq = b
            self._st = self._S_PAY if self._len else self._S_CRC_LO
        elif st == self._S_PAY:
            self._buf.append(b)
            if len(self._buf) >= self._len:
                self._st = self._S_CRC_LO
        elif st == self._S_CRC_LO:
            self._crc_rx = b
            self._st = self._S_CRC_HI
        elif st == self._S_CRC_HI:
            self._crc_rx |= b << 8
            header = bytes((self._type, self._len & 0xFF,
                            (self._len >> 8) & 0xFF, self._seq))
            calc = crc16(header + bytes(self._buf))
            self._st = self._S_SYNC0
            if calc == self._crc_rx:
                return Frame(self._type, self._seq, bytes(self._buf))
            self.dropped += 1
        return None


# --- convenience builders for the common host -> board frames ----------------

def text_frame(text: str, seq: int = 0) -> bytes:
    return encode(Type.TEXT, seq, text.encode("utf-8")[:MAX_PAYLOAD])


def mood_frame(mood: str, seq: int = 0) -> bytes:
    return encode(Type.MOOD, seq, bytes((MOOD_BYTE.get(mood, 0),)))


def state_frame(state: str, seq: int = 0) -> bytes:
    return encode(Type.CMD, seq, bytes((Cmd.STATE, STATE_BYTE.get(state, 0))))


def led_rgb_frame(r: int, g: int, b: int, seq: int = 0) -> bytes:
    return encode(Type.CMD, seq, bytes((Cmd.LED_RGB, r & 0xFF, g & 0xFF, b & 0xFF)))
