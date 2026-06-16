<div align="center">

# Edge AI Voice Assistant

### A real-time, voice-controlled AI assistant on the RP2350 LaunchPad

A resource-constrained microcontroller as the hardware front-end of a modern AI system:
the board owns all real-time I/O, a laptop bridge runs the heavy AI, and the assistant
talks back through the board itself.

![Firmware](https://img.shields.io/badge/Firmware-C%2B%2B%20%2F%20YAHAL-00599C?logo=cplusplus&logoColor=white)
![Bridge](https://img.shields.io/badge/Bridge-Python%20%2F%20FastAPI-3776AB?logo=python&logoColor=white)
![Desktop](https://img.shields.io/badge/App-Native%20(pywebview)-111?logo=apple&logoColor=white)
![LLM](https://img.shields.io/badge/LLM-Groq-7C3AED)
![STT](https://img.shields.io/badge/STT-Vosk%20(offline)-0AA)
![Board](https://img.shields.io/badge/Board-RP2350%20LaunchPad-AE2896)

*Module Mikrocontrollertechnik, FH Aachen, Campus Julich*

</div>

---

## What it does

You hold a button on the board and speak. The board captures your voice, the laptop
bridge transcribes it and asks a language model, and the board answers back: it shows
the reply on its LCD as an animated face, speaks it out loud through its buzzer, and
lights its RGB LEDs in the mood of the answer. Every exchange shows up live in a native
desktop app. An on-device menu (joystick + buttons) lets you drive the board on its own.

Running a full language model on the RP2350 (520 KB SRAM) is not feasible, so this
project adopts the architecture of real edge-AI products (smart speakers): the
**microcontroller handles all real-time hardware I/O**, while the **heavy AI runs
off-device**. The board is the product; the brain is elsewhere, exactly like an Alexa
needs the cloud.

## Features

- **Voice in:** press and hold a button, speak, release. The on-board microphone is
  sampled with the ADC and streamed to the host over a CRC-checked framed protocol.
- **Speech to text:** offline transcription with Vosk (English and French models), no
  cloud and no ffmpeg required.
- **Language model:** Groq (fast cloud, OpenAI-compatible) by default, with Ollama
  (local) and xAI Grok selectable at runtime.
- **The board answers back:** animated LCD face, reply text on the screen, spoken reply
  through the buzzer (PWM audio), and a mood color on the WS2812 RGB LEDs.
- **On-device menu:** a clean menu drawn on the LCD, navigated with the joystick and
  confirmed with the joystick button (Talk, Mood, LEDs, Status, Reset, Close).
- **Native desktop app:** the polished React UI in a real macOS window (no browser),
  with a live conversation view and a control panel to drive the board and switch the AI
  backend.
- **Robust by design:** the firmware never hangs (talk timeout + a Reset menu item),
  the bridge degrades gracefully without a database or a board, and the link
  re-synchronises after lost bytes.

## Architecture

The board and the bridge only know each other through a **framed USB protocol**, so each
side evolves and is tested on its own.

```
                       +-----------------------------+
       speak / type    |     Native desktop app      |  chat, live conversation,
                       |     (React in pywebview)    |  control panel, mood
                       +--------------+--------------+
                                      | HTTP (same-origin)
                                      v
 +------------------------+  UART /   +--------------------------+    +------------------+
 |   RP2350 LaunchPad     |  framed   |   Python bridge          |    |   AI backends    |
 |   (C++ / YAHAL)        |  CRC16    |   (FastAPI)              |--->|   Groq (cloud)   |
 |                        |<--------->|                          |    |   Ollama (local) |
 |  mic ADC capture       |           |   STT -> LLM -> TTS      |    |   Vosk  (STT)    |
 |  LCD animated face     |           |   + board control        |    |   macOS say (TTS)|
 |  WS2812 mood LEDs      |           +-------------+------------+    +------------------+
 |  buzzer speech (PWM)   |                         |
 |  joystick + buttons    |                         v
 +------------------------+              +--------------------------+
                                         |   PostgreSQL (optional)  |  conversation history
                                         +--------------------------+
```

The end-to-end voice loop: **hold the button -> mic ADC capture -> AUDIO_IN frames ->
Vosk transcription -> Groq -> the board shows the mood + reply text and speaks it through
the buzzer, and the desktop app mirrors the whole conversation.**

## Hardware

| Part | Role |
|------|------|
| RP2350 LaunchPad (dual Cortex-M33) | the assistant device, with a built-in CMSIS-DAP debug probe |
| Educational BoosterPack MKII | microphone, 128x128 LCD, joystick, buttons, buzzer |
| On-board WS2812 RGB LEDs (8) | mood lighting (PIO timing) |
| wifiTick add-on (ESP8266) *(optional)* | wireless variant, see below |

## Tech stack

| Layer | Technology |
|-------|------------|
| Firmware | C++ on **YAHAL**, Pico SDK, CMake, ARM GNU toolchain |
| Host bridge | Python, **FastAPI**, `pyserial` |
| Speech to text | **Vosk** (offline) |
| Language model | **Groq** (default), Ollama, xAI Grok |
| Text to speech | macOS `say` (streamed to the board buzzer) |
| Desktop app | **React + TypeScript + Vite** wrapped in **pywebview** |
| Database | PostgreSQL (optional, history) |

## Repository layout

```
Project/
|-- firmware/                RP2350 firmware (YAHAL, CMake)
|   |-- src/
|   |   |-- main.cpp         orchestration: link, mic capture, buzzer, menu
|   |   |-- face.cpp         animated LCD face + reply text screen
|   |   |-- leds.cpp         reactive WS2812 animations
|   |   |-- menu.cpp         on-device joystick menu
|   |   `-- protocol.h       framed CRC16 protocol (shared with the bridge)
|   |-- face_demo/           stand-alone LCD face demo (no host needed)
|   `-- esp8266/groq_bridge/ ESP8266 sketch for the wireless variant
|-- bridge/                  Python host
|   |-- api.py               FastAPI: /chat /history /config /board + voice pipeline
|   |-- app.py               native desktop launcher (pywebview)
|   |-- llm.py stt.py tts.py board_link.py protocol.py
|   `-- tests/               protocol tests
|-- frontend/                React + TypeScript UI (chat + control panel)
|-- db/                      PostgreSQL via Docker (optional)
|-- MASTERPLAN.md            full development plan
`-- README.md
```

## Quick start

### 1. Bridge and AI

```bash
cd bridge
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env          # set EDGEAI_LLM=groq and GROQ_API_KEY
```

The Vosk speech model is downloaded into `bridge/models/`. Get a free Groq key at
<https://console.groq.com/keys>.

### 2. Frontend (once)

```bash
cd frontend && npm install && npm run build
```

### 3. Run the native app

```bash
cd bridge && ./.venv/bin/python app.py
```

A native window opens with the chat, the live conversation, and the control panel. Type
a message, or use the board: hold the talk button and speak.

### 4. Firmware (the board)

This LaunchPad has a built-in debug probe, so it is flashed over SWD with OpenOCD (no
BOOTSEL needed). The reliable command runs OpenOCD at 1 MHz:

```bash
cd firmware && cmake -B build && cmake --build build
openocd -s <YAHAL>/config/board_config_files -f rp2350-launchpad.cfg \
        -c "adapter speed 1000" \
        -c "program build/edge_ai_firmware.elf verify reset exit"
```

After flashing, replug the USB cable once before running the bridge (the probe leaves its
serial bridge in a stale state).

## The framed protocol

A byte-oriented framed protocol over the link, with CRC16 so the receiver can
re-synchronise after noise or lost bytes.

```
SYNC0 SYNC1 | TYPE | LEN_lo LEN_hi | SEQ | PAYLOAD[LEN] | CRC_lo CRC_hi
 0xAA  0x55 | 1 B  |    2 bytes    | 1 B |   N bytes    |  CRC16-CCITT
```

Types: `AUDIO_IN, AUDIO_OUT, TEXT, MOOD, SENSOR_REQ, SENSOR_RSP, CMD, ACK, LOG`. The C++
(`firmware/src/protocol.h`) and Python (`bridge/protocol.py`) implementations are
byte-exact and covered by tests (`bridge/tests/test_protocol.py`).

## Two architectures

- **A. Cable (default, full assistant).** The board talks to the laptop bridge over the
  debug-probe UART. The bridge does STT, the LLM and TTS. This is the complete voice
  assistant: voice in, text and spoken reply out.
- **B. Wireless (wifiTick, text only).** With the ESP8266 add-on the board calls Groq
  directly over WiFi and shows the reply on its screen, with no laptop. Speech in and out
  need the laptop, so the wireless variant is a text assistant driven by the on-device
  menu. The ESP8266 sketch is in `firmware/esp8266/groq_bridge/`.

## Authors

**Saad Fihi** and **Zakaria Sabiri**, Mikrocontrollertechnik, FH Aachen, Campus Julich.

Built on the YAHAL hardware abstraction library.
