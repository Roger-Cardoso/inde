#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_21=R"sql(
CREATE TABLE compositions (
 id TEXT PRIMARY KEY NOT NULL,
 project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
 name TEXT NOT NULL CHECK(length(name)>0),
 revision INTEGER NOT NULL CHECK(revision>0),
 updated_at TEXT NOT NULL,
 payload TEXT NOT NULL CHECK(length(CAST(payload AS BLOB))<=2097152),
 sha256 TEXT NOT NULL CHECK(length(sha256)=64)
);
CREATE INDEX compositions_owner ON compositions(project_id,updated_at DESC,id);
CREATE TABLE composition_outputs (
 document_id TEXT PRIMARY KEY NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
 project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
 composition_id TEXT NOT NULL REFERENCES compositions(id) ON DELETE RESTRICT,
 source_revision INTEGER NOT NULL CHECK(source_revision>0),
 source_sha256 TEXT NOT NULL CHECK(length(source_sha256)=64),
 recipe TEXT NOT NULL CHECK(length(CAST(recipe AS BLOB))<=2097152)
);
)sql";
}
