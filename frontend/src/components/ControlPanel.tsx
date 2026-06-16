import { useEffect, useState } from "react";
import { getConfig, setLlm, sendBoard } from "../api";
import type { Status } from "../types";

const MOOD_COLORS: Record<string, string> = {
  neutral: "#00cccc",
  happy: "#22c55e",
  sad: "#3b82f6",
  angry: "#ef4444",
  curious: "#eab308",
};

const STATE_ICON: Record<string, string> = {
  idle: "○",
  listening: "◉",
  thinking: "◐",
  speaking: "◆",
};

const LED_PRESETS: { label: string; rgb: [number, number, number] }[] = [
  { label: "Red", rgb: [255, 0, 0] },
  { label: "Green", rgb: [0, 255, 0] },
  { label: "Blue", rgb: [0, 0, 255] },
  { label: "White", rgb: [120, 120, 120] },
  { label: "Off", rgb: [0, 0, 0] },
];

export function ControlPanel() {
  const [status, setStatus] = useState<Status | null>(null);
  const [text, setText] = useState("");
  const [err, setErr] = useState(false);
  const [mood, setMood] = useState<string | null>(null);
  const [state, setState] = useState<string | null>(null);
  const [led, setLed] = useState<string | null>(null);

  async function refresh() {
    try {
      setStatus(await getConfig());
      setErr(false);
    } catch {
      setErr(true);
    }
  }

  useEffect(() => {
    refresh();
    const t = setInterval(refresh, 4000);
    return () => clearInterval(t);
  }, []);

  async function switchLlm(llm: string) {
    try {
      setStatus(await setLlm(llm));
    } catch {
      setErr(true);
    }
  }

  async function board(cmd: Parameters<typeof sendBoard>[0]) {
    try {
      await sendBoard(cmd);
    } catch {
      setErr(true);
    }
  }

  const moods = status?.moods ?? ["neutral", "happy", "sad", "angry", "curious"];
  const states = status?.states ?? ["idle", "listening", "thinking", "speaking"];

  return (
    <aside className="cp">
      <div className="cp-head">
        <h2>Control</h2>
        <span className={`cp-live ${status && !err ? "on" : "off"}`}>
          {status && !err ? "live" : "offline"}
        </span>
      </div>

      {err && <div className="cp-banner">Bridge API unreachable</div>}

      {/* Device status card */}
      <div className="cp-card">
        <div className="cp-stat">
          <span className={`cp-dot ${status?.board_connected ? "on" : "off"}`} />
          <span className="cp-stat-k">Board</span>
          <span className="cp-stat-v">{status?.board_connected ? "connected" : "not found"}</span>
        </div>
        <div className="cp-stat">
          <span className={`cp-dot ${status?.database ? "on" : "off"}`} />
          <span className="cp-stat-k">Database</span>
          <span className="cp-stat-v">{status?.database ? "online" : "offline"}</span>
        </div>
        <div className="cp-stat">
          <span className="cp-dot on" />
          <span className="cp-stat-k">AI</span>
          <span className="cp-stat-v">
            {status?.llm ?? "—"} · {status?.llm_mode ?? "—"}
          </span>
        </div>
        <div className="cp-stat">
          <span className={`cp-dot ${status?.tts && status.tts !== "none" ? "on" : "off"}`} />
          <span className="cp-stat-k">Voice</span>
          <span className="cp-stat-v">{status?.tts ?? "—"}</span>
        </div>
      </div>

      {/* AI backend */}
      <h3 className="cp-label">AI backend</h3>
      <div className="cp-seg">
        <button
          className={status?.llm_mode === "groq" ? "active" : ""}
          onClick={() => switchLlm("groq")}
        >
          Groq
        </button>
        <button
          className={status?.llm_mode === "local" ? "active" : ""}
          onClick={() => switchLlm("local")}
        >
          Local
        </button>
        <button
          className={status?.llm_mode === "grok" ? "active" : ""}
          onClick={() => switchLlm("grok")}
        >
          Grok
        </button>
      </div>

      {/* Mood */}
      <h3 className="cp-label">Board mood</h3>
      <div className="cp-grid">
        {moods.map((m) => (
          <button
            key={m}
            className={`cp-tile ${mood === m ? "active" : ""}`}
            style={mood === m ? { borderColor: MOOD_COLORS[m] } : undefined}
            onClick={() => {
              setMood(m);
              board({ mood: m });
            }}
          >
            <span className="cp-swatch" style={{ background: MOOD_COLORS[m] }} />
            {m}
          </button>
        ))}
      </div>

      {/* State */}
      <h3 className="cp-label">Board state</h3>
      <div className="cp-grid four">
        {states.map((s) => (
          <button
            key={s}
            className={`cp-tile ${state === s ? "active" : ""}`}
            onClick={() => {
              setState(s);
              board({ state: s });
            }}
          >
            <span className="cp-ico">{STATE_ICON[s] ?? "•"}</span>
            {s}
          </button>
        ))}
      </div>

      {/* LCD text */}
      <h3 className="cp-label">LCD text</h3>
      <div className="cp-text">
        <input
          value={text}
          placeholder="Show text on the board…"
          onChange={(e) => setText(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter" && text.trim()) board({ text, state: "speaking" });
          }}
        />
        <button disabled={!text.trim()} onClick={() => board({ text, state: "speaking" })}>
          Send
        </button>
      </div>

      {/* LED */}
      <h3 className="cp-label">LED color</h3>
      <div className="cp-grid">
        {LED_PRESETS.map((p) => (
          <button
            key={p.label}
            className={`cp-tile ${led === p.label ? "active" : ""}`}
            onClick={() => {
              setLed(p.label);
              board({ rgb: p.rgb });
            }}
          >
            <span
              className="cp-swatch"
              style={{
                background: `rgb(${p.rgb.join(",")})`,
                border: p.label === "Off" ? "1px solid #3a3f4a" : "none",
              }}
            />
            {p.label}
          </button>
        ))}
      </div>

      <div className="cp-hint">
        Press the board button to talk · the mic streams to the bridge for
        transcription.
      </div>
    </aside>
  );
}
