#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_22=R"sql(
CREATE TABLE entity_aliases (
 id TEXT PRIMARY KEY NOT NULL,
 project_id TEXT NOT NULL REFERENCES projects(id),
 entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
 name TEXT NOT NULL CHECK(length(name)>0),
 normalized TEXT NOT NULL CHECK(length(normalized)>0),
 kind TEXT NOT NULL CHECK(kind IN ('nickname','title','spelling','other')),
 note TEXT NOT NULL, updated_at TEXT NOT NULL, token TEXT NOT NULL,
 UNIQUE(entity_id,normalized)
);
CREATE INDEX entity_aliases_search ON entity_aliases(project_id,normalized,entity_id);
CREATE TABLE fictional_calendars (
 id TEXT PRIMARY KEY NOT NULL,
 project_id TEXT NOT NULL REFERENCES projects(id),
 name TEXT NOT NULL CHECK(length(name)>0),
 payload TEXT NOT NULL CHECK(length(CAST(payload AS BLOB))<=16384),
 sha256 TEXT NOT NULL CHECK(length(sha256)=64)
);
CREATE TABLE axis_calendars (
 axis_id TEXT PRIMARY KEY NOT NULL REFERENCES fictional_time_axes(id) ON DELETE CASCADE,
 project_id TEXT NOT NULL REFERENCES projects(id),
 calendar_id TEXT NOT NULL REFERENCES fictional_calendars(id) ON DELETE RESTRICT,
 epoch_ordinal INTEGER NOT NULL, token TEXT NOT NULL
);
)sql";
}
