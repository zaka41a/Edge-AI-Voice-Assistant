-- Edge AI Voice Assistant - conversation history schema
-- This file is auto-loaded by the Postgres container on first start
-- (mounted into /docker-entrypoint-initdb.d). The bridge also runs
-- the CREATE TABLE IF NOT EXISTS itself, so it works either way.

CREATE TABLE IF NOT EXISTS conversations (
    id          SERIAL PRIMARY KEY,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    mode        TEXT        NOT NULL DEFAULT 'cloud',  -- 'cloud' | 'local' | 'echo-test'
    question    TEXT,                                  -- transcribed user question (STT)
    reply       TEXT,                                  -- language model reply
    mood        TEXT,                                  -- emotional tone shown on the LEDs
    sensors     JSONB,                                 -- snapshot: {"temperature": .., "light": ..}
    latency_ms  INTEGER                                -- end-to-end response time
);

CREATE INDEX IF NOT EXISTS idx_conversations_created_at
    ON conversations (created_at DESC);
