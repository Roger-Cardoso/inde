#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_27 = R"sql(
CREATE TABLE narrative_branch_provenance (
 structure_id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
 source_structure_id TEXT NOT NULL, source_revision TEXT NOT NULL,
 observation_json TEXT NOT NULL CHECK(length(observation_json)<=4194304),
 created_at TEXT NOT NULL,
 FOREIGN KEY(structure_id,project_id) REFERENCES narrative_structures(id,project_id)
 ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX narrative_link_owner ON narrative_links(id,project_id);
CREATE TABLE narrative_dependency_scopes (
 link_id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
 scope TEXT NOT NULL CHECK(scope IN('unclassified','narrative','chronology','writing','editorial')),
 FOREIGN KEY(link_id,project_id) REFERENCES narrative_links(id,project_id)
 ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
)sql";
}
