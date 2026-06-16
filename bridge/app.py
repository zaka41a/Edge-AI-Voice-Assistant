#!/usr/bin/env python3
"""Edge AI Voice Assistant - native local desktop app.

Runs the FastAPI bridge in the background and shows the React UI in a native
macOS window (no browser, no URL bar). Same polished interface as the web app,
but a real standalone local application.

    cd bridge && ./.venv/bin/python app.py

Requires the frontend to be built once (so FastAPI can serve it):

    cd frontend && npm run build
"""
from __future__ import annotations

import threading
import time
import urllib.request

import uvicorn
import webview

HOST, PORT = "127.0.0.1", 8000
URL = f"http://{HOST}:{PORT}"


def _serve() -> None:
    uvicorn.run("api:app", host=HOST, port=PORT, log_level="warning")


def _wait_until_up(timeout: float = 15.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            urllib.request.urlopen(f"{URL}/health", timeout=1)
            return True
        except Exception:  # noqa: BLE001
            time.sleep(0.2)
    return False


def main() -> None:
    threading.Thread(target=_serve, daemon=True).start()
    if not _wait_until_up():
        print("Bridge did not start in time; opening the window anyway.")
    webview.create_window(
        "Edge AI Voice Assistant",
        URL,
        width=1240,
        height=820,
        min_size=(960, 640),
    )
    # allow the microphone (getUserMedia) inside the native webview
    webview.start(private_mode=False)


if __name__ == "__main__":
    main()
