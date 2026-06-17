from __future__ import annotations

import json
import os
import wave

_HERE = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_MODEL = os.path.join(_HERE, "models", "vosk-model-small-fr-0.22")

try:
    from vosk import Model, KaldiRecognizer, SetLogLevel

    SetLogLevel(-1)
except ImportError:
    Model = None

class STT:
    def __init__(self, model_path: str, sample_rate: int = 16000):
        self.model = Model(model_path)
        self.sample_rate = sample_rate

    def transcribe_pcm(self, pcm: bytes, sample_rate: int | None = None) -> str:

        rec = KaldiRecognizer(self.model, sample_rate or self.sample_rate)
        rec.AcceptWaveform(pcm)
        return json.loads(rec.FinalResult()).get("text", "").strip()

    def transcribe_wav(self, path: str) -> str:
        with wave.open(path, "rb") as wf:
            rate = wf.getframerate()
            pcm = wf.readframes(wf.getnframes())
        return self.transcribe_pcm(pcm, rate)

def get_stt() -> STT | None:

    if Model is None:
        return None
    path = os.environ.get("EDGEAI_STT_MODEL", _DEFAULT_MODEL)
    if not os.path.isdir(path):
        return None
    rate = int(os.environ.get("EDGEAI_STT_RATE", "16000"))
    try:
        return STT(path, rate)
    except Exception:
        return None

if __name__ == "__main__":
    import sys

    stt = get_stt()
    if stt is None:
        print("STT unavailable (vosk or model missing).")
        raise SystemExit(1)
    if len(sys.argv) > 1:
        print("transcript:", stt.transcribe_wav(sys.argv[1]))
    else:
        print("STT ready. Pass a WAV file to transcribe.")
