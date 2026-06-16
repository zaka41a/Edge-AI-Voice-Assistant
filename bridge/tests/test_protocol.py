"""Tests for the framed USB protocol (bridge/protocol.py).

Runs with pytest, or stand-alone:  python tests/test_protocol.py
Mirrors the guarantees the firmware decoder must also provide.
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from protocol import (  # noqa: E402
    Decoder, Frame, Type, encode, crc16, mood_frame, text_frame,
    MOOD_BYTE, SYNC0, SYNC1,
)


def decode_all(data: bytes):
    d = Decoder()
    frames = list(d.feed(data))
    return frames, d.dropped


def test_crc16_known_vector():
    # CRC16-CCITT (0xFFFF) over b"123456789" is the well-known 0x29B1.
    assert crc16(b"123456789") == 0x29B1


def test_round_trip_basic():
    raw = encode(Type.TEXT, 7, b"hello board")
    frames, dropped = decode_all(raw)
    assert dropped == 0
    assert len(frames) == 1
    assert frames[0] == Frame(Type.TEXT, 7, b"hello board")


def test_empty_payload():
    raw = encode(Type.SENSOR_REQ, 0, b"")
    frames, dropped = decode_all(raw)
    assert dropped == 0
    assert frames == [Frame(Type.SENSOR_REQ, 0, b"")]


def test_mood_and_text_helpers():
    frames, _ = decode_all(mood_frame("angry", 3))
    assert frames[0].type == Type.MOOD
    assert frames[0].payload == bytes((MOOD_BYTE["angry"],))

    frames, _ = decode_all(text_frame("ok", 1))
    assert frames[0] == Frame(Type.TEXT, 1, b"ok")


def test_multiple_frames_in_one_buffer():
    raw = encode(Type.MOOD, 1, b"\x02") + encode(Type.TEXT, 2, b"hi")
    frames, dropped = decode_all(raw)
    assert dropped == 0
    assert [f.seq for f in frames] == [1, 2]


def test_split_across_reads():
    raw = encode(Type.TEXT, 9, b"streamed in pieces")
    d = Decoder()
    got = []
    # feed byte by byte: the frame must appear exactly once, at the end
    for b in raw:
        got.extend(d.feed(bytes((b,))))
    assert got == [Frame(Type.TEXT, 9, b"streamed in pieces")]
    assert d.dropped == 0


def test_resync_after_leading_garbage():
    garbage = bytes([0x00, 0xFF, SYNC0, 0x13, 0xAA, 0x37])
    raw = garbage + encode(Type.TEXT, 5, b"after noise")
    frames, dropped = decode_all(raw)
    assert frames == [Frame(Type.TEXT, 5, b"after noise")]
    assert dropped > 0


def test_corrupted_crc_is_rejected():
    raw = bytearray(encode(Type.TEXT, 1, b"corruptme"))
    raw[-1] ^= 0xFF  # flip the CRC high byte
    frames, dropped = decode_all(bytes(raw))
    assert frames == []
    assert dropped == 1


def test_corrupted_payload_is_rejected():
    raw = bytearray(encode(Type.TEXT, 1, b"payload"))
    raw[7] ^= 0x01  # flip a payload bit -> CRC mismatch
    frames, dropped = decode_all(bytes(raw))
    assert frames == []
    assert dropped == 1


def test_recovers_after_a_bad_frame():
    bad = bytearray(encode(Type.TEXT, 1, b"bad"))
    bad[-1] ^= 0xFF
    good = encode(Type.TEXT, 2, b"good")
    frames, _ = decode_all(bytes(bad) + good)
    assert frames == [Frame(Type.TEXT, 2, b"good")]


def test_oversize_length_does_not_hang():
    # SYNC + TYPE + LEN=0xFFFF (> MAX_PAYLOAD) must be rejected, then resync.
    bad = bytes([SYNC0, SYNC1, Type.AUDIO_IN, 0xFF, 0xFF, 0x00])
    raw = bad + encode(Type.TEXT, 1, b"ok")
    frames, dropped = decode_all(raw)
    assert frames == [Frame(Type.TEXT, 1, b"ok")]
    assert dropped > 0


def test_fuzz_noise_never_crashes_and_never_invents_frames():
    rng = random.Random(1234)
    d = Decoder()
    invented = 0
    for _ in range(20000):
        invented += len(list(d.feed(bytes([rng.randint(0, 255)]))))
    # Pure noise should essentially never produce a valid CRC frame.
    assert invented <= 1


def test_fuzz_real_frames_survive_random_noise():
    rng = random.Random(7)
    payloads = [bytes(rng.randint(0, 255) for _ in range(rng.randint(0, 64)))
                for _ in range(200)]
    stream = bytearray()
    for i, p in enumerate(payloads):
        # random junk between frames; decoder must still recover every frame
        stream += bytes(rng.randint(0, 255) for _ in range(rng.randint(0, 5)))
        stream += encode(Type.AUDIO_IN, i & 0xFF, p)
    d = Decoder()
    frames = list(d.feed(bytes(stream)))
    assert [f.payload for f in frames] == payloads


def _run_standalone():
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"  ok  {fn.__name__}")
    print(f"\nAll {len(fns)} protocol tests passed.")


if __name__ == "__main__":
    _run_standalone()
