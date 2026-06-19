import type { BoardCmd, ChatResponse, HistoryItem, Status } from "./types";

const BASE = window.location.port === "5173" ? "/api" : "";

export async function sendChat(message: string): Promise<ChatResponse> {
  const res = await fetch(`${BASE}/chat`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ message, mode: "echo-test" }),
  });
  if (!res.ok) throw new Error(`API error ${res.status}`);
  return res.json();
}

export async function fetchHistory(n = 20): Promise<HistoryItem[]> {
  const res = await fetch(`${BASE}/history?n=${n}`);
  if (!res.ok) throw new Error(`API error ${res.status}`);
  return res.json();
}

export async function checkHealth(): Promise<boolean> {
  try {
    const res = await fetch(`${BASE}/health`);
    return res.ok;
  } catch {
    return false;
  }
}

export async function getConfig(): Promise<Status> {
  const res = await fetch(`${BASE}/config`);
  if (!res.ok) throw new Error(`API error ${res.status}`);
  return res.json();
}

export async function setLlm(llm: string): Promise<Status> {
  const res = await fetch(`${BASE}/config`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ llm }),
  });
  if (!res.ok) throw new Error(`API error ${res.status}`);
  return res.json();
}

export async function transcribe(pcm: ArrayBuffer, rate = 16000): Promise<string> {
  const res = await fetch(`${BASE}/transcribe?rate=${rate}`, {
    method: "POST",
    headers: { "Content-Type": "application/octet-stream" },
    body: pcm,
  });
  if (!res.ok) throw new Error(`API error ${res.status}`);
  const data = await res.json();
  return (data.text as string) ?? "";
}

export async function sendBoard(cmd: BoardCmd): Promise<void> {
  const res = await fetch(`${BASE}/board`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(cmd),
  });
  if (!res.ok) throw new Error(`API error ${res.status}`);
}
