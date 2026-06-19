import { useEffect, useRef, useState } from "react";
import { MicRecorder } from "../recorder";
import { transcribe } from "../api";

interface ComposerProps {
  onSend: (text: string) => void;
  disabled?: boolean;
}

export function Composer({ onSend, disabled }: ComposerProps) {
  const [value, setValue] = useState("");
  const [recording, setRecording] = useState(false);
  const [busy, setBusy] = useState(false);
  const recRef = useRef<MicRecorder | null>(null);
  const textareaRef = useRef<HTMLTextAreaElement | null>(null);
  const micOk = MicRecorder.supported();

  useEffect(() => {
    const el = textareaRef.current;
    if (!el) return;
    el.style.height = "auto";
    el.style.height = Math.min(el.scrollHeight, 200) + "px";
  }, [value]);

  function submit() {
    const text = value.trim();
    if (!text || disabled) return;
    onSend(text);
    setValue("");
  }

  function onKeyDown(e: React.KeyboardEvent<HTMLTextAreaElement>) {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      submit();
    }
  }

  async function toggleMic() {
    if (!micOk || busy) return;
    if (!recording) {
      try {
        recRef.current = new MicRecorder();
        await recRef.current.start();
        setRecording(true);
      } catch {
        setRecording(false);
      }
      return;
    }
    setRecording(false);
    setBusy(true);
    try {
      const { pcm, rate } = await recRef.current!.stop();
      const text = await transcribe(pcm, rate);
      if (text) setValue((v) => (v ? v + " " + text : text));
    } catch {
      /* ignore */
    } finally {
      setBusy(false);
    }
  }

  const placeholder = recording
    ? "Listening… tap the mic to stop"
    : busy
    ? "Transcribing…"
    : "Message the Edge AI Assistant…";

  return (
    <div className="composer">
      <div className={`composer-box ${recording ? "listening" : ""}`}>
        <button
          className={`mic-btn ${recording ? "active" : ""}`}
          onClick={toggleMic}
          disabled={!micOk || busy}
          title={micOk ? (recording ? "Stop and transcribe" : "Speak (PC mic)") : "Microphone not available"}
          aria-label="Microphone"
        >
          <MicIcon />
        </button>

        <textarea
          ref={textareaRef}
          className="composer-input"
          placeholder={placeholder}
          value={value}
          rows={1}
          onChange={(e) => setValue(e.target.value)}
          onKeyDown={onKeyDown}
        />

        <button
          className="send-btn"
          onClick={submit}
          disabled={disabled || !value.trim()}
          aria-label="Send"
        >
          <SendIcon />
        </button>
      </div>
      <p className="composer-hint">
        Enter to send · Shift+Enter for a new line
        {micOk ? " · tap the mic to speak with your PC microphone" : ""}
      </p>
    </div>
  );
}

function MicIcon() {
  return (
    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M12 1a3 3 0 0 0-3 3v8a3 3 0 0 0 6 0V4a3 3 0 0 0-3-3z" />
      <path d="M19 10v2a7 7 0 0 1-14 0v-2" />
      <line x1="12" y1="19" x2="12" y2="23" />
      <line x1="8" y1="23" x2="16" y2="23" />
    </svg>
  );
}

function SendIcon() {
  return (
    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <line x1="22" y1="2" x2="11" y2="13" />
      <polygon points="22 2 15 22 11 13 2 9 22 2" />
    </svg>
  );
}
