import type { Message } from "../types";
import { MoodDot } from "./MoodDot";

const MOOD_COLOR: Record<string, string> = {
  neutral: "#00cccc",
  happy: "#22c55e",
  sad: "#3b82f6",
  angry: "#ef4444",
  curious: "#eab308",
};

export function ChatMessage({ message }: { message: Message }) {
  const isUser = message.role === "user";
  const moodColor = (message.mood && MOOD_COLOR[message.mood]) || undefined;

  return (
    <div className={`msg-row ${isUser ? "user" : "assistant"}`}>
      <div className="msg-avatar" aria-hidden>
        {isUser ? "You" : "AI"}
      </div>
      <div
        className="msg-body"
        style={moodColor ? ({ "--mood-c": moodColor } as React.CSSProperties) : undefined}
      >
        <div className="msg-meta">
          <span className="msg-author">{isUser ? "You" : "Edge AI Assistant"}</span>
          {!isUser && message.mood && (
            <span className="msg-chip mood">
              <MoodDot mood={message.mood} />
              {message.mood}
            </span>
          )}
          {!isUser && message.latencyMs != null && (
            <span className="msg-chip">{message.latencyMs} ms</span>
          )}
        </div>
        <div className={`msg-text ${!isUser && moodColor ? "accent" : ""}`}>
          {message.pending ? (
            <span className="typing">
              <span></span>
              <span></span>
              <span></span>
            </span>
          ) : (
            message.text
          )}
        </div>
      </div>
    </div>
  );
}
