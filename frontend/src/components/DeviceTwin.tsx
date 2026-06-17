import { useEffect, useState } from "react";
import { getConfig } from "../api";

const MOOD: Record<string, string> = {
  neutral: "#00cccc",
  happy: "#22c55e",
  sad: "#3b82f6",
  angry: "#ef4444",
  curious: "#eab308",
};

function mouth(mood: string): JSX.Element {
  const c = MOOD[mood] ?? MOOD.neutral;
  if (mood === "happy")
    return <path d="M40 70 Q60 88 80 70" stroke={c} strokeWidth="4" fill="none" strokeLinecap="round" />;
  if (mood === "sad")
    return <path d="M40 80 Q60 64 80 80" stroke={c} strokeWidth="4" fill="none" strokeLinecap="round" />;
  if (mood === "angry")
    return <path d="M42 82 Q60 70 78 82" stroke={c} strokeWidth="4" fill="none" strokeLinecap="round" />;
  if (mood === "curious")
    return <circle cx="60" cy="76" r="7" stroke={c} strokeWidth="4" fill="none" />;
  return <line x1="44" y1="76" x2="76" y2="76" stroke={c} strokeWidth="4" strokeLinecap="round" />;
}

export function DeviceTwin() {
  const [mood, setMood] = useState("neutral");
  const [state, setState] = useState("idle");
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    let alive = true;
    async function tick() {
      try {
        const s = await getConfig();
        if (!alive) return;
        setConnected(s.board_connected);
        if (s.live) {
          setMood(s.live.mood);
          setState(s.live.state);
        }
      } catch {

      }
    }
    tick();
    const t = setInterval(tick, 1000);
    return () => {
      alive = false;
      clearInterval(t);
    };
  }, []);

  const c = MOOD[mood] ?? MOOD.neutral;

  return (
    <div className="twin">
      <div className="twin-head" data-state={state}>
        <svg viewBox="0 0 120 110" className="twin-face">
          {}
          <circle cx="40" cy="44" r="13" fill="#fff" />
          <circle cx="80" cy="44" r="13" fill="#fff" />
          <circle cx="40" cy="44" r="6" fill={c} className="twin-pupil" />
          <circle cx="80" cy="44" r="6" fill={c} className="twin-pupil" />
          {mouth(mood)}
        </svg>
      </div>
      <div className="twin-leds">
        {Array.from({ length: 8 }).map((_, i) => (
          <span
            key={i}
            className="twin-led"
            style={{ background: c, animationDelay: `${i * 90}ms` }}
            data-state={state}
          />
        ))}
      </div>
      <div className="twin-caption">
        <span className="twin-state">{connected ? state : "no board"}</span>
        <span className="twin-mood" style={{ color: c }}>
          {mood}
        </span>
      </div>
    </div>
  );
}
