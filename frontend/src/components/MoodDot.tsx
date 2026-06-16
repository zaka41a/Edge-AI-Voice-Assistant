import type { Mood } from "../types";

const MOOD_COLORS: Record<Mood, string> = {
  neutral: "#8b95a7",
  happy: "#27c093",
  sad: "#4d8bf0",
  angry: "#e5484d",
  curious: "#e0a92b",
};

export function MoodDot({ mood, size = 9 }: { mood: Mood; size?: number }) {
  const color = MOOD_COLORS[mood] ?? MOOD_COLORS.neutral;
  return (
    <span
      title={`mood: ${mood}`}
      style={{
        display: "inline-block",
        width: size,
        height: size,
        borderRadius: "50%",
        background: color,
        boxShadow: `0 0 8px ${color}`,
        flexShrink: 0,
      }}
    />
  );
}
