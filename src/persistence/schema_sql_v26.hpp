#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_26 = R"sql(
CREATE TABLE narrative_unit_contexts (
 unit_id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
 location_entity_id TEXT, writing_position INTEGER CHECK(writing_position>0),
 token TEXT NOT NULL,
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id)
 ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(location_entity_id,project_id) REFERENCES entities(id,project_id)
 ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
);
CREATE TABLE narrative_realizations (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, unit_id TEXT NOT NULL,
 document_id TEXT NOT NULL, note TEXT NOT NULL DEFAULT '',
 UNIQUE(unit_id,document_id),
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id)
 ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id)
 ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX narrative_realization_document ON narrative_realizations(project_id,document_id);
CREATE TABLE narrative_unit_relations (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, unit_id TEXT NOT NULL,
 relation_id TEXT NOT NULL,
 UNIQUE(unit_id,relation_id),
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id)
 ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(relation_id,project_id) REFERENCES relations(id,project_id)
 ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX narrative_unit_relation_owner ON narrative_unit_relations(project_id,relation_id);
CREATE TRIGGER narrative_context_local_insert BEFORE INSERT ON narrative_unit_contexts
WHEN NEW.location_entity_id IS NOT NULL AND NOT EXISTS (
 SELECT 1 FROM entities WHERE id=NEW.location_entity_id AND project_id=NEW.project_id
 AND entity_type_id='00000000-0000-4000-9000-000000000002')
BEGIN SELECT RAISE(ABORT,'unit context requires a Local'); END;
CREATE TRIGGER narrative_context_local_update BEFORE UPDATE ON narrative_unit_contexts
WHEN NEW.location_entity_id IS NOT NULL AND NOT EXISTS (
 SELECT 1 FROM entities WHERE id=NEW.location_entity_id AND project_id=NEW.project_id
 AND entity_type_id='00000000-0000-4000-9000-000000000002')
BEGIN SELECT RAISE(ABORT,'unit context requires a Local'); END;
CREATE TRIGGER narrative_context_local_type BEFORE UPDATE OF entity_type_id ON entities
WHEN NEW.entity_type_id<>'00000000-0000-4000-9000-000000000002' AND EXISTS(
 SELECT 1 FROM narrative_unit_contexts WHERE location_entity_id=OLD.id)
BEGIN SELECT RAISE(ABORT,'unit Local must remain a Local'); END;
)sql";
}
