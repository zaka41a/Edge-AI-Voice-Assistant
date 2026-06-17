import { useEffect, useRef, useState } from "react";
import { Sidebar } from "./components/Sidebar";
import { ChatMessage } from "./components/ChatMessage";
import { Composer } from "./components/Composer";
import { MoodDot } from "./components/MoodDot";
import { ControlPanel } from "./components/ControlPanel";
import { sendChat, fetchHistory, checkHealth } from "./api";
import type { HistoryItem, Message, Mood } from "./types";

function uid() {
  return Math.random().toString(36).slice(2) + Date.now().toString(36);
}

const WELCOME: Message = {
  id: "welcome",
  role: "assistant",
  text:
    "Hi! I'm your Edge AI Voice Assistant. Type a message or tap the mic to speak. " +
    "Every exchange is logged to PostgreSQL.",
  mood: "neutral",
};

export default function App() {
  const [messages, setMessages] = useState<Message[]>([WELCOME]);
  const [history, setHistory] = useState<HistoryItem[]>([]);
  const [dbConnected, setDbConnected] = useState(false);
  const [busy, setBusy] = useState(false);
  const [mood, setMood] = useState<Mood>("neutral");
  const scrollRef = useRef<HTMLDivElement | null>(null);
  const seenRef = useRef<Set<string>>(new Set());

  async function refreshHistory() {
    try {
      setHistory(await fetchHistory(20));
    } catch {

    }
  }

  async function pollVoice() {
    try {
      const items = await fetchHistory(15);
      const fresh = items
        .filter((it) => it.mode?.startsWith("voice") && !seenRef.current.has(it.created_at))
        .reverse();
      if (fresh.length) {
        setMessages((m) => {
          const add: Message[] = [];
          for (const it of fresh) {
            seenRef.current.add(it.created_at);
            add.push({ id: uid(), role: "user", text: "🎤 " + it.question });
            add.push({
              id: uid(),
              role: "assistant",
              text: it.reply,
              mood: it.mood ?? "neutral",
              latencyMs: it.latency_ms ?? undefined,
            });
          }
          return [...m, ...add];
        });
        const last = fresh[fresh.length - 1];
        if (last.mood) setMood(last.mood);
        refreshHistory();
      }
    } catch {

    }
  }

  useEffect(() => {
    checkHealth().then(setDbConnected);

    fetchHistory(50)
      .then((items) => items.forEach((it) => seenRef.current.add(it.created_at)))
      .catch(() => {})
      .finally(() => {
        refreshHistory();
        setInterval(pollVoice, 2500);
      });
  }, []);

  useEffect(() => {
    scrollRef.current?.scrollTo({
      top: scrollRef.current.scrollHeight,
      behavior: "smooth",
    });
  }, [messages]);

  async function handleSend(text: string) {
    const userMsg: Message = { id: uid(), role: "user", text };
    const pendingId = uid();
    setMessages((m) => [
      ...m,
      userMsg,
      { id: pendingId, role: "assistant", text: "", pending: true },
    ]);
    setBusy(true);
    try {
      const res = await sendChat(text);
      setMood(res.mood);
      setMessages((m) =>
        m.map((msg) =>
          msg.id === pendingId
            ? {
                ...msg,
                pending: false,
                text: res.reply,
                mood: res.mood,
                latencyMs: res.latency_ms,
              }
            : msg
        )
      );
      refreshHistory();
    } catch (err) {
      setMessages((m) =>
        m.map((msg) =>
          msg.id === pendingId
            ? {
                ...msg,
                pending: false,
                text:
                  "⚠️ Could not reach the bridge API. Start it with:\n" +
                  "cd bridge && ./.venv/bin/uvicorn api:app --port 8000",
                mood: "sad",
              }
            : msg
        )
      );
    } finally {
      setBusy(false);
    }
  }

  function newChat() {
    setMessages([WELCOME]);
    setMood("neutral");
  }

  return (
    <div className="app">
      <Sidebar history={history} dbConnected={dbConnected} onNewChat={newChat} />

      <main className="chat">
        <header className="chat-header">
          <div className="chat-title">
            <span>Edge AI Voice Assistant</span>
            <span className="chat-subtitle">RP2350 LaunchPad</span>
          </div>
          <div className="chat-mood">
            <span className={`live-pill ${dbConnected ? "on" : "off"}`}>
              {dbConnected ? "online" : "offline"}
            </span>
            <MoodDot mood={mood} />
            <span>{mood}</span>
          </div>
        </header>

        <div className="messages" ref={scrollRef}>
          <div className="messages-inner">
            {messages.map((m) => (
              <ChatMessage key={m.id} message={m} />
            ))}
          </div>
        </div>

        <Composer onSend={handleSend} disabled={busy} />
      </main>

      <ControlPanel />
    </div>
  );
}
