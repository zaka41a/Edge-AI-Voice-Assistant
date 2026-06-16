import type { Message } from "../types";
import { MoodDot } from "./MoodDot";

export function ChatMessage({ message }: { message: Message }) {
  const isUser = message.role === "user";
  return (
    <div className={`msg-row ${isUser ? "user" : "assistant"}`}>
      <div className="msg-avatar" aria-hidden>
        {isUser ? "You" : "AI"}
      </div>
      <div className="msg-body">
        <div className="msg-meta">
          <span className="msg-author">
            {isUser ? "You" : "Edge AI Assistant"}
          </span>
          {!isUser && message.mood && (
            <span className="msg-mood">
              <MoodDot mood={message.mood} />
              {message.mood}
            </span>
          )}
          {!isUser && message.latencyMs != null && (
            <span className="msg-latency">{message.latencyMs} ms</span>
          )}
        </div>
        <div className="msg-text">
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
