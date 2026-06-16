"""Switchable LLM backend for the Edge AI Voice Assistant.

Both Grok (xAI) and Ollama expose an OpenAI-compatible chat API, so a single
client class serves both, parameterized by base URL / key / model:

    EDGEAI_LLM=cloud  -> Grok   (https://api.x.ai/v1,        key XAI_API_KEY)
    EDGEAI_LLM=local  -> Ollama (http://localhost:11434/v1,  no key)

The model is asked to return strict JSON `{"reply": ..., "mood": ...}` so the
assistant's emotional tone can drive the RGB LEDs.
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass

from openai import OpenAI

MOODS = ("neutral", "happy", "sad", "angry", "curious")

SYSTEM_PROMPT = (
    "You are the Edge AI Voice Assistant, running as the front-end of an RP2350 "
    "microcontroller demo. Answer the user concisely and naturally, suitable for "
    "being spoken aloud (1 to 3 sentences). "
    "Always reply in the SAME language as the user's message. "
    "You MUST reply with a single strict JSON object and nothing else, in the form: "
    '{"reply": "<your spoken answer>", "mood": "<one of: '
    + ", ".join(MOODS)
    + '>"}. '
    "The mood is the emotional tone of your reply and controls the colored LEDs."
)


@dataclass
class LLMResult:
    reply: str
    mood: str


class LLM:
    """Common interface."""

    name: str = "llm"

    def chat(self, message: str) -> LLMResult:  # pragma: no cover - interface
        raise NotImplementedError


def _parse(content: str) -> LLMResult:
    """Extract {reply, mood} from a model response, tolerating extra text."""
    text = (content or "").strip()
    try:
        start, end = text.find("{"), text.rfind("}")
        if start != -1 and end != -1:
            obj = json.loads(text[start : end + 1])
            reply = str(obj.get("reply", "")).strip() or text
            mood = str(obj.get("mood", "neutral")).strip().lower()
            return LLMResult(reply, mood if mood in MOODS else "neutral")
    except (ValueError, TypeError):
        pass
    return LLMResult(text or "(no response)", "neutral")


class OpenAICompatLLM(LLM):
    """Works against any OpenAI-compatible endpoint (Grok or Ollama)."""

    def __init__(self, *, name: str, base_url: str, api_key: str,
                 model: str, timeout: float = 45.0):
        self.name = name
        self.model = model
        self._client = OpenAI(base_url=base_url, api_key=api_key, timeout=timeout)

    def chat(self, message: str) -> LLMResult:
        resp = self._client.chat.completions.create(
            model=self.model,
            messages=[
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": message},
            ],
            temperature=0.6,
        )
        return _parse(resp.choices[0].message.content or "")


def get_llm() -> LLM | None:
    """Build the configured LLM, or None if a cloud backend lacks its key.

    EDGEAI_LLM = local | groq | grok (alias: cloud)
      local -> Ollama (on-device, OpenAI-compatible endpoint)
      groq  -> Groq cloud (fast, free tier), key GROQ_API_KEY
      grok  -> xAI Grok, key XAI_API_KEY
    """
    mode = os.environ.get("EDGEAI_LLM", "local").strip().lower()

    if mode == "groq":
        key = os.environ.get("GROQ_API_KEY", "").strip()
        if not key:
            return None
        return OpenAICompatLLM(
            name="groq",
            base_url="https://api.groq.com/openai/v1",
            api_key=key,
            model=os.environ.get("EDGEAI_GROQ_MODEL", "llama-3.3-70b-versatile"),
        )

    if mode in ("cloud", "grok"):
        key = os.environ.get("XAI_API_KEY", "").strip()
        if not key:
            return None
        return OpenAICompatLLM(
            name="grok",
            base_url="https://api.x.ai/v1",
            api_key=key,
            model=os.environ.get("EDGEAI_GROK_MODEL", "grok-2-latest"),
        )

    # default: local Ollama (OpenAI-compatible endpoint, dummy key)
    return OpenAICompatLLM(
        name="ollama",
        base_url=os.environ.get("OLLAMA_BASE_URL", "http://localhost:11434/v1"),
        api_key="ollama",
        model=os.environ.get("EDGEAI_OLLAMA_MODEL", "llama3.1:8b"),
    )
