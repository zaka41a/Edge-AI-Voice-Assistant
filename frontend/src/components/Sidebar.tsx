import type { HistoryItem } from "../types";
import { MoodDot } from "./MoodDot";

interface SidebarProps {
  history: HistoryItem[];
  dbConnected: boolean;
  onNewChat: () => void;
}

export function Sidebar({ history, dbConnected, onNewChat }: SidebarProps) {
  return (
    <aside className="sidebar">
      <div className="sidebar-head">
        <div className="brand">
          <span className="brand-mark">◆</span>
          <span className="brand-name">Edge AI</span>
        </div>
        <button className="new-chat" onClick={onNewChat}>
          + New chat
        </button>
      </div>

      <div className="sidebar-section-title">History</div>
      <div className="history-list">
        {history.length === 0 && (
          <div className="history-empty">No conversations yet</div>
        )}
        {history.map((h, i) => (
          <div className="history-item" key={i} title={h.reply}>
            {h.mood && <MoodDot mood={h.mood} size={8} />}
            <span className="history-q">{h.question || "(empty)"}</span>
          </div>
        ))}
      </div>

      <div className="sidebar-foot">
        <StatusLine label="Database" ok={dbConnected} />
        <StatusLine label="Board (USB)" ok={false} pending />
        <StatusLine label="AI pipeline" ok={false} pending />
      </div>
    </aside>
  );
}

function StatusLine({
  label,
  ok,
  pending,
}: {
  label: string;
  ok: boolean;
  pending?: boolean;
}) {
  const cls = ok ? "ok" : pending ? "pending" : "down";
  const text = ok ? "online" : pending ? "soon" : "offline";
  return (
    <div className="status-line">
      <span className={`status-dot ${cls}`} />
      <span className="status-label">{label}</span>
      <span className="status-text">{text}</span>
    </div>
  );
}
