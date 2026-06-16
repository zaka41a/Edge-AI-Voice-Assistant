import { useEffect, useRef, useState } from "react";
import { Microphone, isSpeechSupported } from "../speech";

interface ComposerProps {
  onSend: (text: string) => void;
  disabled?: boolean;
}

export function Composer({ onSend, disabled }: ComposerProps) {
  const [value, setValue] = useState("");
  const [listening, setListening] = useState(false);
  const micRef = useRef<Microphone | null>(null);
  const baseRef = useRef("");
  const textareaRef = useRef<HTMLTextAreaElement | null>(null);
  const speechOk = isSpeechSupported();

  // Auto-grow the textarea
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

  function toggleMic() {
    if (!speechOk) return;
    if (!micRef.current) {
      micRef.current = new Microphone({
        onResult: (transcript, isFinal) => {
          setValue((baseRef.current + " " + transcript).trim());
          if (isFinal) baseRef.current = (baseRef.current + " " + transcript).trim();
        },
        onEnd: () => setListening(false),
        onError: () => setListening(false),
      });
    }
    if (listening) {
      micRef.current.stop();
      setListening(false);
    } else {
      baseRef.current = value;
      micRef.current.start();
      setListening(true);
    }
  }

  return (
    <div className="composer">
      <div className={`composer-box ${listening ? "listening" : ""}`}>
        <button
          className={`mic-btn ${listening ? "active" : ""}`}
          onClick={toggleMic}
          disabled={!speechOk}
          title={
            speechOk
              ? listening
                ? "Stop listening"
                : "Speak"
              : "Speech recognition not supported in this browser"
          }
          aria-label="Microphone"
        >
          <MicIcon />
        </button>

        <textarea
          ref={textareaRef}
          className="composer-input"
          placeholder={listening ? "Listening…" : "Message the Edge AI Assistant…"}
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
        {speechOk ? " · tap the mic to dictate" : ""}
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
