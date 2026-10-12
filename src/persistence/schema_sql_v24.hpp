#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_24=R"sql(
CREATE UNIQUE INDEX temporal_references_owner ON temporal_references(id,project_id);
CREATE UNIQUE INDEX relations_temporal_owner ON relations(id,project_id);
CREATE TABLE temporal_attachments (
 id TEXT PRIMARY KEY NOT NULL,
 project_id TEXT NOT NULL REFERENCES projects(id),
 reference_id TEXT NOT NULL,
 entity_id TEXT, relation_id TEXT, unit_id TEXT, line_id TEXT,
 role TEXT NOT NULL CHECK(length(role)>0 AND length(role)<=128),
 note TEXT NOT NULL CHECK(length(note)<=2048), token TEXT NOT NULL,
 CHECK((entity_id IS NOT NULL)+(relation_id IS NOT NULL)+(unit_id IS NOT NULL)+(line_id IS NOT NULL)=1),
 FOREIGN KEY(reference_id,project_id) REFERENCES temporal_references(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(entity_id,project_id) REFERENCES entities(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(relation_id,project_id) REFERENCES relations(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(line_id,project_id) REFERENCES narrative_lines(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX temporal_attachment_unique ON temporal_attachments(reference_id,role,
 coalesce(entity_id,''),coalesce(relation_id,''),coalesce(unit_id,''),coalesce(line_id,''));
CREATE INDEX temporal_attachment_reference ON temporal_attachments(project_id,reference_id,id);
CREATE INDEX temporal_attachment_entity ON temporal_attachments(project_id,entity_id);
CREATE INDEX temporal_attachment_relation ON temporal_attachments(project_id,relation_id);
CREATE INDEX temporal_attachment_unit ON temporal_attachments(project_id,unit_id);
CREATE INDEX temporal_attachment_line ON temporal_attachments(project_id,line_id);
)sql";
}
