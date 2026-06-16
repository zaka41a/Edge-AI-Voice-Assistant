#!/usr/bin/env python3
"""Edge AI Voice Assistant - local desktop control app (no web, no browser).

A standalone Tkinter window that runs on the laptop and drives the RP2350 board
directly over the back-channel UART. From here you can:

  - chat with the assistant (LLM -> reply + mood), and have the BOARD show it
  - manually set the board mood / state / LCD text / LED color
  - switch the AI backend (local Ollama <-> cloud Grok)
  - browse the PostgreSQL conversation history

Everything degrades gracefully: no board, no LLM or no database just disables
the matching parts. Run:

    cd bridge && ./.venv/bin/python desktop_app.py
"""
from __future__ import annotations

import os
import threading
import time
import tkinter as tk
from tkinter import ttk

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

import board_link
import llm as llm_mod
import tts as tts_mod

try:
    from db import Database
except Exception:  # noqa: BLE001
    Database = None  # type: ignore

DSN = os.environ.get("EDGEAI_DSN", "postgresql://edgeai:edgeai@localhost:5544/edgeai")

MOODS = ["neutral", "happy", "sad", "angry", "curious"]
STATES = ["idle", "listening", "thinking", "speaking"]
MOOD_COLOR = {
    "neutral": "#00b3b3", "happy": "#22c55e", "sad": "#3b82f6",
    "angry": "#ef4444", "curious": "#d4a017",
}
LED_PRESETS = [
    ("Red", (255, 0, 0)), ("Green", (0, 255, 0)), ("Blue", (0, 0, 255)),
    ("White", (120, 120, 120)), ("Off", (0, 0, 0)),
]

BG = "#15171c"
PANEL = "#1b1e25"
FG = "#e7e9ee"
DIM = "#9aa3b2"


class DesktopApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("Edge AI Voice Assistant - Control")
        root.configure(bg=BG)
        root.geometry("780x640")
        root.minsize(680, 560)

        # --- backends -----------------------------------------------------
        self.board = board_link.BoardLink()
        self.llm = llm_mod.get_llm()
        self.llm_mode = tk.StringVar(value=os.environ.get("EDGEAI_LLM", "local"))
        self.laptop_voice = tk.BooleanVar(value=False)
        self.tts = tts_mod.get_tts()
        self.db = None
        if Database is not None:
            try:
                self.db = Database(DSN)
            except Exception:  # noqa: BLE001
                self.db = None

        self._build_styles()
        self._build_ui()
        if self.board.connected:
            self.board.set_state("idle")
        self.refresh_status()

    # ------------------------------------------------------------------ UI
    def _build_styles(self) -> None:
        st = ttk.Style()
        try:
            st.theme_use("clam")
        except tk.TclError:
            pass
        st.configure("TFrame", background=BG)
        st.configure("Panel.TFrame", background=PANEL)
        st.configure("TLabel", background=BG, foreground=FG)
        st.configure("Dim.TLabel", background=BG, foreground=DIM, font=("Helvetica", 10))
        st.configure("H.TLabel", background=BG, foreground=DIM,
                     font=("Helvetica", 9, "bold"))
        st.configure("Title.TLabel", background=BG, foreground=FG,
                     font=("Helvetica", 15, "bold"))
        st.configure("TButton", padding=5)
        st.configure("TRadiobutton", background=BG, foreground=FG)
        st.configure("TCheckbutton", background=BG, foreground=FG)

    def _section(self, parent, title) -> ttk.Frame:
        ttk.Label(parent, text=title.upper(), style="H.TLabel").pack(
            anchor="w", pady=(12, 4))
        f = ttk.Frame(parent)
        f.pack(fill="x")
        return f

    def _build_ui(self) -> None:
        # Header
        head = ttk.Frame(self.root)
        head.pack(fill="x", padx=16, pady=(14, 0))
        ttk.Label(head, text="Edge AI Voice Assistant", style="Title.TLabel").pack(side="left")
        self.status_lbl = ttk.Label(head, text="", style="Dim.TLabel")
        self.status_lbl.pack(side="right")

        body = ttk.Frame(self.root)
        body.pack(fill="both", expand=True, padx=16, pady=8)
        left = ttk.Frame(body)
        left.pack(side="left", fill="both", expand=True, padx=(0, 10))
        right = ttk.Frame(body, width=300)
        right.pack(side="right", fill="y")
        right.pack_propagate(False)

        # --- left: chat ---------------------------------------------------
        ttk.Label(left, text="CONVERSATION", style="H.TLabel").pack(anchor="w")
        self.log = tk.Text(left, bg=PANEL, fg=FG, insertbackground=FG,
                           relief="flat", wrap="word", height=18,
                           font=("Helvetica", 12), padx=10, pady=10)
        self.log.pack(fill="both", expand=True, pady=(4, 8))
        self.log.tag_config("you", foreground="#4f8cff", font=("Helvetica", 12, "bold"))
        self.log.tag_config("ai", foreground="#22c55e", font=("Helvetica", 12, "bold"))
        self.log.tag_config("meta", foreground=DIM, font=("Helvetica", 9))
        self.log.configure(state="disabled")

        row = ttk.Frame(left)
        row.pack(fill="x")
        self.entry = tk.Entry(row, bg="#1e2229", fg=FG, insertbackground=FG,
                              relief="flat", font=("Helvetica", 12))
        self.entry.pack(side="left", fill="x", expand=True, ipady=6, padx=(0, 8))
        self.entry.bind("<Return>", lambda e: self.on_send())
        self.send_btn = ttk.Button(row, text="Send", command=self.on_send)
        self.send_btn.pack(side="right")

        # --- right: controls ---------------------------------------------
        f = self._section(right, "AI backend")
        ttk.Radiobutton(f, text="Local (Ollama)", value="local",
                        variable=self.llm_mode, command=self.on_llm).pack(anchor="w")
        ttk.Radiobutton(f, text="Cloud (Grok)", value="cloud",
                        variable=self.llm_mode, command=self.on_llm).pack(anchor="w")
        ttk.Checkbutton(f, text="Also speak on laptop", variable=self.laptop_voice).pack(
            anchor="w", pady=(4, 0))

        f = self._section(right, "Board mood")
        self._chip_row(f, [(m, MOOD_COLOR[m], lambda m=m: self.board.set_mood(m))
                           for m in MOODS])

        f = self._section(right, "Board state")
        self._chip_row(f, [(s, None, lambda s=s: self.board.set_state(s))
                           for s in STATES])

        f = self._section(right, "LCD text")
        self.text_entry = tk.Entry(f, bg="#1e2229", fg=FG, insertbackground=FG,
                                   relief="flat", font=("Helvetica", 11))
        self.text_entry.pack(fill="x", ipady=5)
        self.text_entry.bind("<Return>", lambda e: self.on_text())
        ttk.Button(f, text="Show on board", command=self.on_text).pack(
            fill="x", pady=(6, 0))

        f = self._section(right, "LED color")
        self._chip_row(f, [(name, "rgb(%d,%d,%d)" % rgb,
                            lambda rgb=rgb: self.board.set_led_rgb(*rgb))
                           for name, rgb in LED_PRESETS])

    def _chip_row(self, parent, items) -> None:
        # NOTE: macOS Tk ignores tk.Button bg, so the button face stays light.
        # Use DARK text so labels are always readable, and a colored swatch
        # (which does respect bg) as the color cue.
        wrap = ttk.Frame(parent)
        wrap.pack(fill="x")
        for i, (label, color, cmd) in enumerate(items):
            cell = tk.Frame(wrap, bg=PANEL)
            cell.grid(row=i // 3, column=i % 3, padx=3, pady=3, sticky="ew")
            if color:
                sw = tk.Label(cell, text=" ", bg=(color if color.startswith("#")
                                                  else self._rgb_to_hex(color)),
                              width=1)
                sw.pack(side="left", padx=(0, 4))
            b = tk.Button(cell, text=label, command=cmd, relief="raised",
                          fg="#15171c", activeforeground="#000000",
                          font=("Helvetica", 10, "bold"), padx=6, pady=3)
            b.pack(side="left", fill="x", expand=True)
        for c in range(3):
            wrap.columnconfigure(c, weight=1)

    @staticmethod
    def _rgb_to_hex(rgbstr: str) -> str:
        # "rgb(255,0,0)" -> "#ff0000"
        nums = rgbstr[rgbstr.find("(") + 1:rgbstr.find(")")].split(",")
        r, g, b = (int(n) for n in nums)
        return f"#{r:02x}{g:02x}{b:02x}"

    # --------------------------------------------------------------- logic
    def refresh_status(self) -> None:
        board = "board OK" if self.board.connected else "no board"
        db = "db OK" if self.db is not None else "no db"
        llm = self.llm.name if self.llm is not None else "no LLM"
        self.status_lbl.configure(text=f"{board}  ·  {llm} ({self.llm_mode.get()})  ·  {db}")
        self.root.after(4000, self.refresh_status)

    def on_llm(self) -> None:
        os.environ["EDGEAI_LLM"] = self.llm_mode.get()
        self.llm = llm_mod.get_llm()
        self.refresh_status()

    def on_text(self) -> None:
        txt = self.text_entry.get().strip()
        if txt:
            self.board.send_text(txt)
            self.board.set_state("speaking")

    def _append(self, who: str, text: str, tag: str, meta: str = "") -> None:
        self.log.configure(state="normal")
        self.log.insert("end", f"{who} ", tag)
        self.log.insert("end", text + "\n")
        if meta:
            self.log.insert("end", meta + "\n", "meta")
        self.log.insert("end", "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def on_send(self) -> None:
        msg = self.entry.get().strip()
        if not msg or self.send_btn["state"] == "disabled":
            return
        self.entry.delete(0, "end")
        self._append("You:", msg, "you")
        self.send_btn.configure(state="disabled")
        self.board.set_state("thinking")
        threading.Thread(target=self._ask, args=(msg,), daemon=True).start()

    def _ask(self, msg: str) -> None:
        t0 = time.time()
        if self.llm is not None:
            try:
                res = self.llm.chat(msg)
                reply, mood, mode = res.reply, res.mood, self.llm.name
            except Exception as exc:  # noqa: BLE001
                reply, mood, mode = f"(LLM error: {exc})", "sad", "error"
        else:
            reply, mood, mode = "(no LLM configured)", "neutral", "none"
        latency = int((time.time() - t0) * 1000)

        # present on the board
        self.board.set_mood(mood)
        self.board.send_text(reply)
        self.board.set_state("speaking")
        if self.laptop_voice.get() and self.tts is not None:
            self.tts.speak(reply)
        self.board.set_state("idle")

        if self.db is not None:
            try:
                self.db.log(mode=mode, question=msg, reply=reply, mood=mood,
                            sensors={}, latency_ms=latency)
            except Exception:  # noqa: BLE001
                pass

        self.root.after(0, lambda: self._done(reply, mood, latency))

    def _done(self, reply: str, mood: str, latency: int) -> None:
        self._append("Assistant:", reply, "ai", f"mood: {mood}  ·  {latency} ms")
        self.send_btn.configure(state="normal")


def main() -> None:
    root = tk.Tk()
    DesktopApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
