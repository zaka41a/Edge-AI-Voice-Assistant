export type Role = "user" | "assistant";

export type Mood =
  | "neutral"
  | "happy"
  | "sad"
  | "angry"
  | "curious";

export interface Message {
  id: string;
  role: Role;
  text: string;
  mood?: Mood;
  latencyMs?: number;
  pending?: boolean;
}

export interface ChatResponse {
  reply: string;
  mood: Mood;
  latency_ms: number;
}

export interface HistoryItem {
  created_at: string;
  mode: string;
  question: string;
  reply: string;
  mood: Mood | null;
  latency_ms: number | null;
}

export interface Status {
  status: string;
  database: boolean;
  llm: string | null;
  llm_mode: string;
  board_connected: boolean;
  tts: string;
  moods: string[];
  states: string[];
}

export interface BoardCmd {
  mood?: string;
  state?: string;
  text?: string;
  rgb?: [number, number, number];
}
