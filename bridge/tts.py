"""Text-to-speech for the Edge AI Voice Assistant (host side).

Gives the assistant a real voice. The default backend uses macOS's built-in
`say` command: zero install, fully offline, instant, which makes for a robust
live demo. The interface is pluggable so Piper (local, cross-platform) or a
cloud TTS can be added later without touching the callers.

    EDGEAI_TTS=say     -> macOS `say` (default on macOS)
    EDGEAI_TTS=none    -> disabled (no audio)

Voice and rate can be set with EDGEAI_TTS_VOICE and EDGEAI_TTS_RATE.
"""
from __future__ import annotations

import os
import shutil
import subprocess


class TTS:
    """Common interface."""
    name = "tts"

    def speak(self, text: str) -> None:        # blocking: returns when spoken
        raise NotImplementedError

    def available(self) -> bool:
        return True


class NullTTS(TTS):
    name = "none"

    def speak(self, text: str) -> None:
        pass


class MacSayTTS(TTS):
    """macOS `say`: offline, no dependencies."""
    name = "say"

    def __init__(self, voice: str | None = None, rate: int | None = None):
        self.voice = voice or os.environ.get("EDGEAI_TTS_VOICE", "Thomas")
        self.rate = rate or int(os.environ.get("EDGEAI_TTS_RATE", "190"))

    def available(self) -> bool:
        return shutil.which("say") is not None

    def speak(self, text: str) -> None:
        if not text.strip():
            return
        cmd = ["say", "-r", str(self.rate)]
        if self.voice:
            cmd += ["-v", self.voice]
        cmd.append(text)
        try:
            subprocess.run(cmd, check=False)
        except OSError:
            pass


def synth_pcm8(text: str, rate: int = 8000, voice: str | None = None) -> bytes | None:
    """Render text to 8-bit signed mono PCM at `rate` Hz, for the board buzzer.

    Uses macOS `say` + `afconvert`. Returns None if unavailable or on failure.
    """
    import audioop
    import tempfile
    import wave

    if not text.strip() or shutil.which("say") is None or shutil.which("afconvert") is None:
        return None
    voice = voice or os.environ.get("EDGEAI_TTS_VOICE", "Daniel")
    aiff = tempfile.mktemp(suffix=".aiff")
    wav = tempfile.mktemp(suffix=".wav")
    try:
        subprocess.run(["say", "-v", voice, "-o", aiff, text], check=False)
        subprocess.run(["afconvert", "-f", "WAVE", "-d", f"LEI16@{rate}", "-c", "1",
                        aiff, wav], check=False)
        with wave.open(wav, "rb") as wf:
            pcm16 = wf.readframes(wf.getnframes())
        # Maximise loudness for the small buzzer: normalise, then over-drive so
        # the signal saturates (compression). audioop saturates on overflow, so
        # this lifts the average level -> much louder (a bit distorted, fine here).
        peak = audioop.max(pcm16, 2)
        if peak > 0:
            drive = float(os.environ.get("EDGEAI_BUZZER_DRIVE", "3.5"))
            pcm16 = audioop.mul(pcm16, 2, (32000.0 / peak) * drive)
        return audioop.lin2lin(pcm16, 2, 1)          # 16-bit -> 8-bit signed
    except Exception:  # noqa: BLE001
        return None
    finally:
        for f in (aiff, wav):
            try:
                os.remove(f)
            except OSError:
                pass


def get_tts() -> TTS:
    """Build the configured TTS backend, falling back gracefully."""
    mode = os.environ.get("EDGEAI_TTS", "say").strip().lower()
    if mode == "none":
        return NullTTS()
    # default: macOS say (fall back to null if unavailable)
    say = MacSayTTS()
    return say if say.available() else NullTTS()


if __name__ == "__main__":
    import sys
    t = get_tts()
    print(f"TTS backend: {t.name}")
    t.speak(" ".join(sys.argv[1:]) or "Bonjour, je suis ton assistant Edge AI.")
