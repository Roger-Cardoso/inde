#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_23 = R"sql(
CREATE TABLE calendar_revisions (
 calendar_id TEXT PRIMARY KEY NOT NULL REFERENCES fictional_calendars(id) ON DELETE RESTRICT,
 project_id TEXT NOT NULL REFERENCES projects(id),
 parent_id TEXT NOT NULL REFERENCES fictional_calendars(id) ON DELETE RESTRICT,
 CHECK(calendar_id<>parent_id)
);
CREATE TABLE calendar_adoptions (
 id TEXT PRIMARY KEY NOT NULL,
 project_id TEXT NOT NULL REFERENCES projects(id),
 axis_id TEXT NOT NULL REFERENCES fictional_time_axes(id) ON DELETE RESTRICT,
 calendar_id TEXT NOT NULL REFERENCES fictional_calendars(id) ON DELETE RESTRICT,
 from_ordinal INTEGER NOT NULL, epoch_ordinal INTEGER NOT NULL,
 mode TEXT NOT NULL CHECK(mode IN ('retire','coexist')),
 UNIQUE(axis_id,from_ordinal)
);
CREATE INDEX calendar_adoptions_axis ON calendar_adoptions(project_id,axis_id,from_ordinal);
CREATE TABLE temporal_references (
 id TEXT PRIMARY KEY NOT NULL,
 project_id TEXT NOT NULL REFERENCES projects(id),
 axis_id TEXT NOT NULL REFERENCES fictional_time_axes(id) ON DELETE RESTRICT,
 calendar_id TEXT NOT NULL REFERENCES fictional_calendars(id) ON DELETE RESTRICT,
 label TEXT NOT NULL CHECK(length(label)>0), year INTEGER NOT NULL,
 month INTEGER, day INTEGER, clock TEXT NOT NULL CHECK(length(clock)<=128),
 note TEXT NOT NULL, token TEXT NOT NULL,
 CHECK(day IS NULL OR month IS NOT NULL)
);
CREATE INDEX temporal_references_axis ON temporal_references(project_id,axis_id,label,id);
)sql";
}
