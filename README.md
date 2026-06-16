<div align="center">

# Edge AI Voice Assistant

### Real-time, voice-controlled AI assistant on the RP2350 LaunchPad

A resource-constrained microcontroller as the hardware front-end of a modern AI system:
the board owns all real-time I/O, the laptop runs the heavy AI, PostgreSQL keeps the memory.

![Firmware](https://img.shields.io/badge/Firmware-C%2B%2B%20%2F%20YAHAL-00599C?logo=cplusplus&logoColor=white)
![Bridge](https://img.shields.io/badge/Bridge-Python%20%2F%20FastAPI-3776AB?logo=python&logoColor=white)
![Frontend](https://img.shields.io/badge/Frontend-React%20%2F%20TypeScript-61DAFB?logo=react&logoColor=black)
![Database](https://img.shields.io/badge/Database-PostgreSQL%2016-4169E1?logo=postgresql&logoColor=white)
![LLM](https://img.shields.io/badge/LLM-Grok%20%7C%20Ollama-7C3AED)
![Board](https://img.shields.io/badge/Board-RP2350%20LaunchPad-AE2896)

</div>

---

## Overview

The user speaks to the board; the board replies with a spoken answer, an on-screen
message on the LCD, and a colored mood on its WS2812 RGB LEDs. Running a full language
model on the RP2350 (520 KB SRAM) is not feasible, so we adopt the architecture of real
commercial edge-AI products (smart speakers): the **microcontroller handles all
real-time hardware I/O**, while the **heavy AI inference runs off-device** on a laptop
bridge. To keep genuine intelligence at the edge, a lightweight wake-word and
voice-activity detection stage still runs on the board itself.

> Full requirements: [`Project_Proposal/Lastenheft.pdf`](../Project_Proposal/Lastenheft.pdf)
> · Development plan: [`ROADMAP.md`](ROADMAP.md)

## Architecture

```
                ┌──────────────────────┐
   speak / type │     React Web UI     │ chatbot , mic, mood, history
                └──────────┬───────────┘
                           │ HTTP (/api proxy)
                           ▼
┌───────────────┐   USB    ┌───────────────────────┐   ┌───────────────────────┐
│  RP2350 board │  CDC ACM │   Python bridge       │   │   AI backends         │
│  (C++ / YAHAL)│◄────────►│   (FastAPI)           │──►│   Grok (cloud)        │
│               │  framed  │                       │   │   Ollama (local)      │
│ mic ADC+DMA   │  proto.  │   STT -> LLM -> TTS   │   │   Whisper / Piper     │
│ I2S audio out │          │                       │   └───────────────────────┘
│ WS2812 / LCD  │          └──────────┬────────────┘
│ wake-word/VAD │                     │
└───────────────┘                     ▼
                            ┌──────────────────────┐
                            │   PostgreSQL         │  conversation history
                            └──────────────────────┘
```

## Features

- **Voice in, voice out**: microphone capture on the board, spoken reply playback.
- **Multimodal feedback**: reply shown on the LCD, emotional tone on RGB LEDs.
- **Switchable AI**: cloud (Grok / xAI) or fully local (Ollama), one flag to swap.
- **Persistent memory**: every exchange logged to PostgreSQL (question, reply, mood,
  sensor snapshot, latency).
- **Professional web client**: ChatGPT-style React UI with a working microphone button.
- **Edge intelligence**: on-device wake-word and VAD so audio streams only on demand.

## Tech stack

| Layer        | Technology                                                        |
|--------------|-------------------------------------------------------------------|
| Firmware     | C++ on **YAHAL**, Pico SDK, CMake, ARM GNU toolchain              |
| Host bridge  | Python, **FastAPI**, `pyserial`, `psycopg` (v3)                   |
| AI (LLM)     | **Grok** (xAI, OpenAI-compatible) · **Ollama** (local)            |
| AI (speech)  | Whisper / faster-whisper (STT) · Piper (TTS)                      |
| Web client   | **React 18** + **TypeScript** + **Vite**, Web Speech API          |
| Database     | **PostgreSQL 16** (Docker)                                         |

## Repository layout

```
Project/
├── firmware/      C++ firmware for the RP2350 (YAHAL, CMake)
│   └── src/main.cpp        USB CDC ACM echo + LED heartbeat (skeleton)
├── bridge/        Python host bridge
│   ├── api.py              FastAPI web API (/chat, /history, /health)
│   ├── bridge.py           USB serial link CLI + DB logging
│   └── db.py               PostgreSQL access layer
├── frontend/      React + TypeScript chat UI (ChatGPT-style, mic)
├── db/            PostgreSQL via Docker (docker-compose.yml, schema.sql)
├── ROADMAP.md     Phased development plan
└── README.md
```

---

## Quick start

### Prerequisites

| Tool                 | Used for                          |
|----------------------|-----------------------------------|
| Docker               | PostgreSQL database               |
| Python 3.11+         | host bridge + API                 |
| Node 18+ / npm       | React frontend                    |
| CMake + ARM GNU GCC  | building the firmware (board only)|
| Ollama *(optional)*  | local LLM backend                 |

### 1. Database

```bash
cd db
docker compose up -d        # PostgreSQL on localhost:5544 (user/pass/db = edgeai)
```

### 2. Bridge API

```bash
cd bridge
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env        # set EDGEAI_LLM, XAI_API_KEY, etc.

uvicorn api:app --reload --port 8000
```

### 3. Web UI

```bash
cd frontend
npm install
npm run dev                 # http://localhost:5173
```

Type a message or tap the mic. The request flows through the Vite `/api` proxy to the
FastAPI bridge, gets a reply + mood from the LLM, and is logged to PostgreSQL.

### 4. Firmware *(requires the board)*

```bash
cd firmware
cmake -B build && cmake --build build
# flash the generated .uf2 via BOOTSEL / picotool
```

The firmware auto-locates YAHAL at `../../yahal_with_examples/YAHAL`
(override with `export YAHAL_DIR=...`). The red LED blinks once USB is enumerated.

---

## Configuration

The bridge reads its settings from environment variables (see `bridge/.env.example`):

| Variable               | Default                                            | Description                          |
|------------------------|----------------------------------------------------|--------------------------------------|
| `EDGEAI_LLM`           | `local`                                            | AI backend: `cloud` (Grok) or `local`|
| `XAI_API_KEY`          | *(none)*                                           | Grok API key (required for `cloud`)  |
| `EDGEAI_GROK_MODEL`    | `grok-2-latest`                                    | Grok model id                        |
| `OLLAMA_BASE_URL`      | `http://localhost:11434/v1`                        | Ollama OpenAI-compatible endpoint    |
| `EDGEAI_OLLAMA_MODEL`  | `llama3.1:8b`                                       | local model name                     |
| `EDGEAI_DSN`           | `postgresql://edgeai:edgeai@localhost:5544/edgeai` | PostgreSQL DSN                       |

## HTTP API

| Method | Endpoint    | Description                                             |
|--------|-------------|--------------------------------------------------------|
| `GET`  | `/health`   | Liveness + database/LLM status                         |
| `POST` | `/chat`     | `{ "message": "..." }` -> `{ reply, mood, latency_ms }`|
| `GET`  | `/history`  | `?n=20` recent interactions                            |

## Database schema

Table `conversations`:

| Column       | Type          | Description                                  |
|--------------|---------------|----------------------------------------------|
| `id`         | `SERIAL` (PK) | unique interaction id                        |
| `created_at` | `TIMESTAMPTZ` | time of the interaction                      |
| `mode`       | `TEXT`        | backend used (`cloud` / `local` / `echo`)    |
| `question`   | `TEXT`        | transcribed user question                    |
| `reply`      | `TEXT`        | language model reply                         |
| `mood`       | `TEXT`        | emotional tone shown on the LEDs             |
| `sensors`    | `JSONB`       | sensor snapshot (temperature, light, ...)    |
| `latency_ms` | `INTEGER`     | end-to-end response time                     |

---

## Roadmap

Development is organized in phases (see [`ROADMAP.md`](ROADMAP.md)):

| Phase | Focus                                  | Board needed |
|-------|----------------------------------------|:------------:|
| 1     | Foundation (scaffold, USB, DB, UI)     | no  **done** |
| 2     | Brain on the laptop (LLM, STT, TTS)    |      no      |
| 3     | Firmware I/O + framed USB protocol     |     yes      |
| 4     | Full integration                       |     yes      |
| 5     | Edge AI (wake-word, VAD, sensors)      |     yes      |
| 6     | Polish, offline mode, tests, demo      |     yes      |

## Authors

**Saad Fihi** and **Zakaria Sabiri** Mikrocontrollertechnik module,
FH Aachen, Campus Jülich.
