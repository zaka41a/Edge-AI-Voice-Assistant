from __future__ import annotations

import os
import shutil
import subprocess

class TTS:

    name = "tts"

    def speak(self, text: str) -> None:
        raise NotImplementedError

    def available(self) -> bool:
        return True

class NullTTS(TTS):
    name = "none"

    def speak(self, text: str) -> None:
        pass

class MacSayTTS(TTS):

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

def synth_pcm8(text: str, rate: int = 16000, voice: str | None = None) -> bytes | None:

    import tempfile
    import wave

    import numpy as np

    if not text.strip() or shutil.which("say") is None or shutil.which("afconvert") is None:
        return None
    voice = voice or os.environ.get("EDGEAI_TTS_VOICE", "Daniel")
    say_rate = os.environ.get("EDGEAI_TTS_RATE", "160")
    aiff = tempfile.mktemp(suffix=".aiff")
    wav = tempfile.mktemp(suffix=".wav")
    try:
        subprocess.run(["say", "-v", voice, "-r", say_rate, "-o", aiff, text], check=False)
        subprocess.run(["afconvert", "-f", "WAVE", "-d", f"LEI16@{rate}", "-c", "1",
                        aiff, wav], check=False)
        with wave.open(wav, "rb") as wf:
            raw = wf.readframes(wf.getnframes())

        x = np.frombuffer(raw, dtype="<i2").astype(np.float32)
        if x.size == 0:
            return None

        pre = float(os.environ.get("EDGEAI_BUZZER_PRE", "0.85"))
        x = np.append(x[0], x[1:] - pre * x[:-1])

        a = 0.97
        y = np.empty_like(x)
        acc = 0.0
        for i, s in enumerate(x):
            acc = a * acc + (1 - a) * s
            y[i] = s - acc
        x = y

        peak = np.max(np.abs(x)) or 1.0
        drive = float(os.environ.get("EDGEAI_BUZZER_DRIVE", "3.0"))
        x = np.tanh((x / peak) * drive)
        x = x / (np.max(np.abs(x)) or 1.0)

        pcm8 = np.clip(np.round(x * 127.0), -128, 127).astype(np.int8)
        return pcm8.tobytes()
    except Exception:
        return None
    finally:
        for f in (aiff, wav):
            try:
                os.remove(f)
            except OSError:
                pass

def get_tts() -> TTS:

    mode = os.environ.get("EDGEAI_TTS", "say").strip().lower()
    if mode == "none":
        return NullTTS()

    say = MacSayTTS()
    return say if say.available() else NullTTS()

if __name__ == "__main__":
    import sys
    t = get_tts()
    print(f"TTS backend: {t.name}")
    t.speak(" ".join(sys.argv[1:]) or "Bonjour, je suis ton assistant Edge AI.")
