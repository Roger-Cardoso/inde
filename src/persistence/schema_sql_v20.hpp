#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_20 = R"sql(
CREATE TABLE project_operations (
 sequence INTEGER PRIMARY KEY AUTOINCREMENT,
 id TEXT NOT NULL UNIQUE,
 project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
 capture_project_id TEXT NOT NULL,
 label TEXT NOT NULL CHECK(length(label)>0),
 created_at TEXT NOT NULL,
 state TEXT NOT NULL DEFAULT 'applied' CHECK(state IN ('applied','reverted')),
 reversed_at TEXT,
 is_compensation INTEGER NOT NULL DEFAULT 0 CHECK(is_compensation IN (0,1)),
 trash_operation_id TEXT,
 changes BLOB NOT NULL CHECK(length(changes)>0 AND length(changes)<=8388608),
 sha256 TEXT NOT NULL CHECK(length(sha256)=64),
 CHECK((state='applied' AND reversed_at IS NULL) OR (state='reverted' AND reversed_at IS NOT NULL))
);
CREATE INDEX project_operations_history ON project_operations(project_id,sequence DESC);
CREATE INDEX project_operations_trash ON project_operations(project_id,trash_operation_id);
)sql";
}
