-- QG MVP schema (DESIGN.md §21.2).
--
-- Canonical text for migration 1. It is compiled into the binary at configure
-- time (schema_sql.hpp.in) so there is exactly one copy. All statements are
-- idempotent (IF NOT EXISTS) so re-running the migration is harmless.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);

-- known paths <-> stable identities (rename-safe state)
CREATE TABLE IF NOT EXISTS paths (
  path       TEXT PRIMARY KEY,
  identity   TEXT NOT NULL,
  dev        INTEGER,
  ino        INTEGER,
  size       INTEGER,
  mtime_ns   INTEGER,
  seen_at    INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_paths_identity ON paths(identity);

-- application metadata keyed by identity
CREATE TABLE IF NOT EXISTS media_state (
  identity         TEXT PRIMARY KEY,
  favorite         INTEGER NOT NULL DEFAULT 0,
  watched          INTEGER NOT NULL DEFAULT 0,
  watch_count      INTEGER NOT NULL DEFAULT 0,
  last_position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms      INTEGER,
  width            INTEGER,
  height           INTEGER,
  poster_ms        INTEGER,
  updated_at       INTEGER NOT NULL
);

-- thumbnail references / failure markers
CREATE TABLE IF NOT EXISTS thumbnails (
  identity        TEXT PRIMARY KEY,
  source_mtime_ns INTEGER,
  source_size     INTEGER,
  cache_path      TEXT,
  thumb_ms        INTEGER,
  state           TEXT NOT NULL,          -- 'ok' | 'failed'
  fail_count      INTEGER NOT NULL DEFAULT 0,
  generated_at    INTEGER NOT NULL
);

-- recent locations (capped by the application)
CREATE TABLE IF NOT EXISTS recent_locations (
  path       TEXT PRIMARY KEY,
  visited_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS bookmarks (
  id       INTEGER PRIMARY KEY,
  path     TEXT NOT NULL UNIQUE,
  label    TEXT,
  position INTEGER NOT NULL DEFAULT 0,
  kind     TEXT NOT NULL DEFAULT 'folder'   -- future: 'collection'
);

CREATE TABLE IF NOT EXISTS settings (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

-- future (created now, unused in MVP): collections, tags, ratings
CREATE TABLE IF NOT EXISTS collections (
  id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS collection_items (
  collection_id INTEGER NOT NULL REFERENCES collections(id),
  identity      TEXT NOT NULL,
  added_at      INTEGER NOT NULL,
  PRIMARY KEY (collection_id, identity)
);
