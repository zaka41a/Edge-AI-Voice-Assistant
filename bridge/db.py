from __future__ import annotations

import psycopg
from psycopg.types.json import Jsonb

SCHEMA = """
CREATE TABLE IF NOT EXISTS conversations (
    id          SERIAL PRIMARY KEY,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    mode        TEXT        NOT NULL DEFAULT 'cloud',
    question    TEXT,
    reply       TEXT,
    mood        TEXT,
    sensors     JSONB,
    latency_ms  INTEGER
);
"""

class Database:
    def __init__(self, dsn: str):

        self.conn = psycopg.connect(dsn, autocommit=True)
        self.conn.execute(SCHEMA)

    def log(self, *, mode: str, question: str | None, reply: str | None,
            mood: str | None, sensors: dict | None, latency_ms: int | None) -> None:
        self.conn.execute(
            """
            INSERT INTO conversations (mode, question, reply, mood, sensors, latency_ms)
            VALUES (%s, %s, %s, %s, %s, %s)
            """,
            (mode, question, reply, mood, Jsonb(sensors or {}), latency_ms),
        )

    def recent(self, n: int = 10):
        cur = self.conn.execute(
            """
            SELECT created_at, mode, question, reply, mood, latency_ms
            FROM conversations
            ORDER BY id DESC
            LIMIT %s
            """,
            (n,),
        )
        return cur.fetchall()

    def close(self) -> None:
        self.conn.close()
