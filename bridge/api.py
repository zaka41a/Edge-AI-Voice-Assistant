#!/usr/bin/env python3
"""HTTP API for the Edge AI Voice Assistant web front-end.

Exposes the bridge to the React UI:
    POST /chat      -> send a message, get a reply + mood, logged to Postgres
    GET  /history   -> recent conversation history
    GET  /health    -> liveness + DB status

Run:
    cd bridge
    ./.venv/bin/uvicorn api:app --reload --port 8000

The AI pipeline (STT/LLM/TTS) is not wired yet (later milestone); for now
/chat echoes the message and derives a simple mood, so the full
UI -> API -> PostgreSQL path is real and demonstrable.
"""
from __future__ import annotations

import os
import threading
import time

try:
    from dotenv import load_dotenv

    load_dotenv()
except ImportError:  # python-dotenv is optional
    pass

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from db import Database
from llm import get_llm
from tts import get_tts
from stt import get_stt
from board_link import BoardLink

DSN = os.environ.get(
    "EDGEAI_DSN", "postgresql://edgeai:edgeai@localhost:5544/edgeai"
)

app = FastAPI(title="Edge AI Voice Assistant API", version="0.1.0")

# Allow the Vite dev server (and others) to call the API during development
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Single shared DB connection (autocommit). None if Postgres is unavailable.
try:
    _db: Database | None = Database(DSN)
except Exception as exc:  # noqa: BLE001
    print(f"WARNING: PostgreSQL unavailable ({exc}); history disabled.")
    _db = None

# Configured LLM backend (Grok or Ollama). None -> falls back to echo.
_llm = get_llm()
_llm_mode = os.environ.get("EDGEAI_LLM", "local").lower()

# Link to the RP2350 board. The BOARD itself displays the reply on its LCD and
# (next step) speaks it; the laptop only generates the data. Degrades
# gracefully: no board connected -> these cues are simply skipped.
_board = BoardLink()
if _board.connected:
    _board.set_state("idle")

# Optional laptop-side TTS, OFF by default (the board is meant to be the voice).
# Enable with EDGEAI_TTS=say only if you want the laptop to speak too.
_tts = get_tts() if os.environ.get("EDGEAI_TTS", "none").lower() != "none" else None


def _present_reply(reply: str, mood: str) -> None:
    """Drive the board to present the reply: show its mood, push the reply text
    to the LCD, and SPEAK it on the board's buzzer (audio streamed from here).
    Runs in a background thread so the HTTP response returns immediately."""
    from tts import synth_pcm8

    _board.set_mood(mood)
    _board.send_text(reply)
    _board.set_state("speaking")

    pcm8 = synth_pcm8(reply) if _board.connected else None
    if pcm8:
        _board.play_audio(pcm8)          # the BOARD speaks via its buzzer
    elif _tts is not None:
        _tts.speak(reply)                # fallback: laptop voice

    _board.set_state("idle")


# Recent exchanges kept in memory, so the UI shows the conversation (incl. voice)
# even when PostgreSQL is not running.
_recent: list[dict] = []


def _remember(mode: str, question: str, reply: str, mood: str, latency: int) -> None:
    from datetime import datetime, timezone
    _recent.append({
        "created_at": datetime.now(timezone.utc).isoformat(),
        "mode": mode, "question": question, "reply": reply,
        "mood": mood, "latency_ms": latency,
    })
    del _recent[:-100]  # keep the last 100


def _run_pipeline(question: str, mode: str) -> dict:
    """Shared question -> reply pipeline used by both typed chat and voice."""
    t0 = time.time()
    if _llm is not None:
        try:
            r = _llm.chat(question)
            reply, mood, used = r.reply, r.mood, _llm.name
        except Exception as exc:  # noqa: BLE001
            reply, mood, used = f"(LLM unavailable: {exc})", "sad", f"{_llm.name}-error"
    else:
        reply, mood, used = f"(echo) {question}", detect_mood(question), "echo"
    latency = int((time.time() - t0) * 1000)
    mode = mode if mode else used
    threading.Thread(target=_present_reply, args=(reply, mood), daemon=True).start()
    _remember(mode, question, reply, mood, latency)
    if _db is not None:
        _db.log(mode=mode, question=question, reply=reply,
                mood=mood, sensors={}, latency_ms=latency)
    return {"reply": reply, "mood": mood, "latency_ms": latency}


# Speech-to-text + push-to-talk: when the board streams captured mic audio, we
# transcribe it (Vosk), run it through the same pipeline, and the board reacts.
_stt = get_stt()


def _on_voice(pcm: bytes, rate: int, stats: dict | None = None) -> None:
    import audioop
    import wave

    here = os.path.dirname(os.path.abspath(__file__))
    # The board streams 8-bit signed samples (to halve the link load); expand
    # back to 16-bit for the recogniser.
    pcm = audioop.lin2lin(pcm, 1, 2)
    dur = len(pcm) / 2 / rate
    rms = audioop.rms(pcm, 2) if pcm else 0

    # Boost weak captures toward a target level before recognition.
    boosted = pcm
    if rms:
        gain = min(8.0, max(1.0, 2500.0 / rms))
        boosted = audioop.mul(pcm, 2, gain)

    # Save what the board captured so we can LISTEN to it.
    try:
        with wave.open(os.path.join(here, "last_capture.wav"), "wb") as wf:
            wf.setnchannels(1); wf.setsampwidth(2); wf.setframerate(rate)
            wf.writeframes(boosted)
    except Exception:  # noqa: BLE001
        pass

    text = _stt.transcribe_pcm(boosted, rate) if _stt is not None else ""

    line = (f"[voice] {dur:.2f}s rms={rms} frames={(stats or {}).get('frames')} "
            f"dropped={(stats or {}).get('dropped')} "
            f"crc_dropped={(stats or {}).get('decoder_dropped')} "
            f"transcript={text!r}")
    print(line)
    try:
        with open(os.path.join(here, "voice_debug.log"), "a") as fh:
            fh.write(line + "\n")
    except Exception:  # noqa: BLE001
        pass

    if text.strip():
        _run_pipeline(text, mode=f"voice:{_llm.name if _llm else 'echo'}")
    else:
        # Don't leave the board stuck on "thinking": acknowledge and reset.
        _board.send_text("Sorry, I did not catch that.")
        _board.set_mood("sad")
        _board.set_state("idle")


if _board.connected and _stt is not None:
    _board.start_reader(_on_voice)


# --- simple placeholder mood detection until the real LLM provides it -------
_MOOD_KEYWORDS = {
    "happy": ("thanks", "great", "cool", "love", "nice", "awesome", ":)"),
    "sad": ("sorry", "bad", "error", "fail", "broken", ":("),
    "angry": ("stupid", "hate", "wrong", "annoying"),
    "curious": ("how", "why", "what", "when", "where", "?"),
}


def detect_mood(text: str) -> str:
    low = text.lower()
    for mood, words in _MOOD_KEYWORDS.items():
        if any(w in low for w in words):
            return mood
    return "neutral"


class ChatIn(BaseModel):
    message: str
    mode: str = "echo-test"


class ChatOut(BaseModel):
    reply: str
    mood: str
    latency_ms: int


class ConfigIn(BaseModel):
    llm: str | None = None          # "cloud" | "local"


class BoardCmd(BaseModel):
    mood: str | None = None         # neutral|happy|sad|angry|curious
    state: str | None = None        # idle|listening|thinking|speaking
    text: str | None = None         # caption text for the LCD
    rgb: list[int] | None = None    # [r, g, b] solid LED color


MOODS = ["neutral", "happy", "sad", "angry", "curious"]
STATES = ["idle", "listening", "thinking", "speaking"]


def _status() -> dict:
    return {
        "status": "ok",
        "database": _db is not None,
        "llm": _llm.name if _llm is not None else None,
        "llm_mode": _llm_mode,
        "board_connected": _board.connected,
        "tts": _tts.name if _tts is not None else "none",
        "moods": MOODS,
        "states": STATES,
    }


@app.get("/health")
def health() -> dict:
    return _status()


@app.get("/config")
def get_config() -> dict:
    return _status()


@app.post("/config")
def set_config(cfg: ConfigIn) -> dict:
    """Switch the LLM backend (local / groq / grok) at runtime."""
    global _llm, _llm_mode
    if cfg.llm in ("local", "groq", "grok", "cloud"):
        os.environ["EDGEAI_LLM"] = cfg.llm
        _llm = get_llm()
        _llm_mode = cfg.llm
    return _status()


@app.post("/board")
def board_cmd(cmd: BoardCmd) -> dict:
    """Manually drive the board from the control panel."""
    if cmd.mood:
        _board.set_mood(cmd.mood)
    if cmd.text is not None:
        _board.send_text(cmd.text)
    if cmd.rgb and len(cmd.rgb) == 3:
        _board.set_led_rgb(*cmd.rgb)
    if cmd.state:
        _board.set_state(cmd.state)
    return {"ok": True, "board_connected": _board.connected}


@app.post("/chat", response_model=ChatOut)
def chat(inp: ChatIn) -> ChatOut:
    _board.set_state("thinking")  # board reacts while the LLM is working
    out = _run_pipeline(inp.message, mode="")
    return ChatOut(**out)


@app.get("/history")
def history(n: int = 20) -> list[dict]:
    if _db is None:
        # No database: serve the in-memory recent exchanges (newest first).
        return list(reversed(_recent[-n:]))
    rows = _db.recent(n)
    return [
        {
            "created_at": created_at.isoformat(),
            "mode": mode,
            "question": question,
            "reply": reply,
            "mood": mood,
            "latency_ms": latency_ms,
        }
        for created_at, mode, question, reply, mood, latency_ms in rows
    ]


# --- serve the built React frontend (for the packaged desktop app) -----------
# Mounted last so the API routes above keep priority. `html=True` serves
# index.html for the root and any client-side route.
_FRONTEND_DIST = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "frontend", "dist"
)
if os.path.isdir(_FRONTEND_DIST):
    from fastapi.staticfiles import StaticFiles

    app.mount("/", StaticFiles(directory=_FRONTEND_DIST, html=True), name="frontend")
