#include "inde/persistence/schema_migrator.hpp"

#include "inde/persistence/sqlite_database.hpp"

#include <stdexcept>

namespace inde::persistence {
namespace {

constexpr auto schema_version_15 = R"sql(
ALTER TABLE cartographic_planets ADD COLUMN terrain_revision INTEGER NOT NULL
  DEFAULT 0 CHECK(terrain_revision>=0);
CREATE TABLE cartographic_terrain_locks (
  id TEXT PRIMARY KEY CHECK(length(id)>0),
  project_id TEXT NOT NULL,
  planet_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK(length(name)>0 AND length(name)<=256),
  longitude_e6 INTEGER NOT NULL CHECK(longitude_e6 BETWEEN -180000000 AND 179999999),
  latitude_e6 INTEGER NOT NULL CHECK(latitude_e6 BETWEEN -90000000 AND 90000000),
  radius_e6 INTEGER NOT NULL CHECK(radius_e6 BETWEEN 500000 AND 15000000),
  created_at TEXT NOT NULL CHECK(length(created_at)>0),
  UNIQUE(id,planet_id),
  FOREIGN KEY(planet_id,project_id) REFERENCES cartographic_planets(id,project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX cartographic_terrain_locks_planet_idx
  ON cartographic_terrain_locks(project_id,planet_id,created_at,id);
)sql";

constexpr auto create_migration_table = R"sql(
CREATE TABLE IF NOT EXISTS schema_migrations (
  version INTEGER PRIMARY KEY CHECK (version > 0),
  applied_at TEXT NOT NULL CHECK (length(applied_at) > 0)
)
)sql";

constexpr auto schema_version_5 = R"sql(
CREATE TABLE entity_presences (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  location_entity_id TEXT NOT NULL,
  start_time_point_id TEXT NOT NULL,
  end_time_point_id TEXT,
  description TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  CHECK (entity_id <> location_entity_id),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (location_entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (start_time_point_id, project_id)
    REFERENCES fictional_time_points(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (end_time_point_id, project_id)
    REFERENCES fictional_time_points(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX entity_presences_identity_interval_idx
  ON entity_presences(
    project_id, entity_id, location_entity_id, start_time_point_id,
    COALESCE(end_time_point_id, '')
  );
CREATE INDEX entity_presences_entity_idx
  ON entity_presences(project_id, entity_id, start_time_point_id, id);
CREATE INDEX entity_presences_location_idx
  ON entity_presences(project_id, location_entity_id, start_time_point_id, id);

CREATE TRIGGER entity_presences_location_insert_guard
BEFORE INSERT ON entity_presences
WHEN (SELECT entity_type_id FROM entities WHERE id = NEW.location_entity_id)
       <> '00000000-0000-4000-9000-000000000002' BEGIN
  SELECT RAISE(ABORT, 'presence location must be a location entity');
END;
CREATE TRIGGER entity_presences_location_update_guard
BEFORE UPDATE OF location_entity_id ON entity_presences
WHEN (SELECT entity_type_id FROM entities WHERE id = NEW.location_entity_id)
       <> '00000000-0000-4000-9000-000000000002' BEGIN
  SELECT RAISE(ABORT, 'presence location must be a location entity');
END;
CREATE TRIGGER entity_presences_time_insert_guard
BEFORE INSERT ON entity_presences
WHEN NEW.end_time_point_id IS NOT NULL AND (
  (SELECT axis_id FROM fictional_time_points WHERE id = NEW.start_time_point_id)
    <> (SELECT axis_id FROM fictional_time_points WHERE id = NEW.end_time_point_id)
  OR
  (SELECT ordinal FROM fictional_time_points WHERE id = NEW.start_time_point_id)
    > (SELECT ordinal FROM fictional_time_points WHERE id = NEW.end_time_point_id)
) BEGIN
  SELECT RAISE(ABORT, 'presence interval is not ordered on one fictional axis');
END;
CREATE TRIGGER entity_presences_time_update_guard
BEFORE UPDATE OF start_time_point_id, end_time_point_id ON entity_presences
WHEN NEW.end_time_point_id IS NOT NULL AND (
  (SELECT axis_id FROM fictional_time_points WHERE id = NEW.start_time_point_id)
    <> (SELECT axis_id FROM fictional_time_points WHERE id = NEW.end_time_point_id)
  OR
  (SELECT ordinal FROM fictional_time_points WHERE id = NEW.start_time_point_id)
    > (SELECT ordinal FROM fictional_time_points WHERE id = NEW.end_time_point_id)
) BEGIN
  SELECT RAISE(ABORT, 'presence interval is not ordered on one fictional axis');
END;
CREATE TRIGGER entities_presence_location_type_update_guard
BEFORE UPDATE OF entity_type_id ON entities
WHEN EXISTS (
       SELECT 1 FROM entity_presences WHERE location_entity_id = OLD.id)
 AND NEW.entity_type_id <> '00000000-0000-4000-9000-000000000002' BEGIN
  SELECT RAISE(ABORT, 'presence location must remain a location entity');
END;
CREATE TRIGGER entity_presences_time_point_update_guard
BEFORE UPDATE OF axis_id, ordinal ON fictional_time_points
WHEN EXISTS (
  SELECT 1 FROM entity_presences
  WHERE start_time_point_id = OLD.id OR end_time_point_id = OLD.id
) BEGIN
  SELECT RAISE(ABORT, 'time point used by a presence cannot change axis or order');
END;
)sql";

constexpr auto schema_version_6 = R"sql(
CREATE TABLE entity_work_scopes (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  work_id TEXT NOT NULL,
  notes TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (entity_id, work_id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (work_id) REFERENCES works(id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX entity_work_scopes_entity_idx
  ON entity_work_scopes(project_id, entity_id, work_id, id);
CREATE INDEX entity_work_scopes_work_idx
  ON entity_work_scopes(project_id, work_id, entity_id, id);

CREATE TRIGGER entity_work_scopes_project_insert_guard
BEFORE INSERT ON entity_work_scopes
WHEN (SELECT ip.project_id FROM works w
      JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
      WHERE w.id = NEW.work_id) <> NEW.project_id BEGIN
  SELECT RAISE(ABORT, 'work scope must belong to the active project');
END;

CREATE TABLE editorial_entity_references (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  work_id TEXT NOT NULL,
  editorial_node_id TEXT NOT NULL,
  purpose TEXT NOT NULL CHECK (length(purpose) > 0),
  notes TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (entity_id, editorial_node_id, purpose),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_id, work_id, project_id)
    REFERENCES entity_work_scopes(entity_id, work_id, project_id)
    ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (editorial_node_id, work_id)
    REFERENCES editorial_nodes(id, work_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX editorial_entity_references_entity_idx
  ON editorial_entity_references(project_id, entity_id, work_id, id);
CREATE INDEX editorial_entity_references_node_idx
  ON editorial_entity_references(project_id, editorial_node_id, entity_id, id);
)sql";

constexpr auto schema_version_7 = R"sql(
CREATE TABLE documents (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  editorial_node_id TEXT,
  title TEXT NOT NULL CHECK (length(title) BETWEEN 1 AND 512),
  content TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (editorial_node_id) REFERENCES editorial_nodes(id)
    ON DELETE SET NULL DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX documents_project_updated_idx
  ON documents(project_id, updated_at DESC, id);
CREATE INDEX documents_project_title_idx
  ON documents(project_id, title COLLATE NOCASE, id);
CREATE INDEX documents_editorial_node_idx
  ON documents(project_id, editorial_node_id, id);

CREATE TRIGGER documents_editorial_project_insert_guard
BEFORE INSERT ON documents
WHEN NEW.editorial_node_id IS NOT NULL AND (
  SELECT ip.project_id FROM editorial_nodes n
  JOIN works w ON w.id = n.work_id
  JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
  WHERE n.id = NEW.editorial_node_id
) <> NEW.project_id BEGIN
  SELECT RAISE(ABORT, 'document placement must belong to the active project');
END;
CREATE TRIGGER documents_editorial_project_update_guard
BEFORE UPDATE OF editorial_node_id, project_id ON documents
WHEN NEW.editorial_node_id IS NOT NULL AND (
  SELECT ip.project_id FROM editorial_nodes n
  JOIN works w ON w.id = n.work_id
  JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
  WHERE n.id = NEW.editorial_node_id
) <> NEW.project_id BEGIN
  SELECT RAISE(ABORT, 'document placement must belong to the active project');
END;
)sql";

constexpr auto schema_version_8 = R"sql(
ALTER TABLE documents ADD COLUMN word_goal INTEGER
  CHECK (word_goal IS NULL OR word_goal BETWEEN 1 AND 10000000);

CREATE TABLE document_format_spans (
  project_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  style TEXT NOT NULL CHECK (style IN (
    'bold', 'italic', 'underline', 'strikethrough',
    'heading', 'subheading', 'quote')),
  start_offset INTEGER NOT NULL CHECK (start_offset >= 0),
  end_offset INTEGER NOT NULL CHECK (end_offset > start_offset),
  PRIMARY KEY (document_id, style, start_offset, end_offset),
  FOREIGN KEY (document_id, project_id) REFERENCES documents(id, project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX document_format_spans_project_idx
  ON document_format_spans(project_id, document_id, start_offset);

CREATE TABLE document_anchors (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  label TEXT NOT NULL CHECK (length(label) BETWEEN 1 AND 256),
  start_offset INTEGER NOT NULL CHECK (start_offset >= 0),
  end_offset INTEGER NOT NULL CHECK (end_offset >= start_offset),
  UNIQUE (id, project_id),
  FOREIGN KEY (document_id, project_id) REFERENCES documents(id, project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX document_anchors_document_idx
  ON document_anchors(project_id, document_id, start_offset, id);

CREATE TABLE document_entity_references (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  anchor_id TEXT,
  notes TEXT NOT NULL DEFAULT '' CHECK (length(notes) <= 4096),
  UNIQUE (id, project_id),
  FOREIGN KEY (document_id, project_id) REFERENCES documents(id, project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_id, project_id) REFERENCES entities(id, project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (anchor_id) REFERENCES document_anchors(id)
    ON DELETE SET NULL DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX document_entity_reference_target_idx
  ON document_entity_references(
    document_id, entity_id, COALESCE(anchor_id, ''));
CREATE INDEX document_entity_references_entity_idx
  ON document_entity_references(project_id, entity_id, document_id);
CREATE INDEX document_entity_references_document_idx
  ON document_entity_references(project_id, document_id, entity_id);

CREATE TRIGGER document_entity_anchor_insert_guard
BEFORE INSERT ON document_entity_references
WHEN NEW.anchor_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM document_anchors a
  WHERE a.id = NEW.anchor_id AND a.project_id = NEW.project_id
    AND a.document_id = NEW.document_id
) BEGIN
  SELECT RAISE(ABORT, 'document entity reference anchor mismatch');
END;
CREATE TRIGGER document_entity_anchor_update_guard
BEFORE UPDATE OF anchor_id, project_id, document_id
ON document_entity_references
WHEN NEW.anchor_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM document_anchors a
  WHERE a.id = NEW.anchor_id AND a.project_id = NEW.project_id
    AND a.document_id = NEW.document_id
) BEGIN
  SELECT RAISE(ABORT, 'document entity reference anchor mismatch');
END;
)sql";

constexpr auto schema_version_9 = R"sql(
CREATE TABLE document_groups (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) BETWEEN 1 AND 256),
  description TEXT NOT NULL DEFAULT '' CHECK (length(description) <= 2048),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (project_id, name COLLATE NOCASE),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX document_groups_project_name_idx
  ON document_groups(project_id, name COLLATE NOCASE, id);

ALTER TABLE documents ADD COLUMN group_id TEXT
  REFERENCES document_groups(id) ON DELETE SET NULL
  DEFERRABLE INITIALLY DEFERRED;
ALTER TABLE documents ADD COLUMN purpose TEXT NOT NULL DEFAULT 'main-text'
  CHECK (purpose IN ('main-text', 'annotation', 'revision', 'outline',
                     'research', 'reference', 'other'));
ALTER TABLE documents ADD COLUMN revision_of_id TEXT
  REFERENCES documents(id) ON DELETE SET NULL
  DEFERRABLE INITIALLY DEFERRED;
ALTER TABLE documents ADD COLUMN revision_label TEXT NOT NULL DEFAULT ''
  CHECK (length(revision_label) <= 128);
ALTER TABLE documents ADD COLUMN perspective TEXT NOT NULL DEFAULT ''
  CHECK (length(perspective) <= 256);
CREATE INDEX documents_group_idx
  ON documents(project_id, group_id, updated_at DESC, id);
CREATE INDEX documents_purpose_idx
  ON documents(project_id, purpose, updated_at DESC, id);
CREATE INDEX documents_revision_idx
  ON documents(project_id, revision_of_id, updated_at DESC, id);

CREATE TRIGGER documents_group_project_insert_guard
BEFORE INSERT ON documents
WHEN NEW.group_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM document_groups g
  WHERE g.id = NEW.group_id AND g.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'document group must belong to the active project');
END;
CREATE TRIGGER documents_group_project_update_guard
BEFORE UPDATE OF group_id, project_id ON documents
WHEN NEW.group_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM document_groups g
  WHERE g.id = NEW.group_id AND g.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'document group must belong to the active project');
END;
CREATE TRIGGER documents_revision_insert_guard
BEFORE INSERT ON documents
WHEN NEW.revision_of_id IS NOT NULL AND (
  NEW.revision_of_id = NEW.id OR NOT EXISTS (
    SELECT 1 FROM documents d
    WHERE d.id = NEW.revision_of_id AND d.project_id = NEW.project_id
  )
) BEGIN
  SELECT RAISE(ABORT, 'invalid document revision origin');
END;
CREATE TRIGGER documents_revision_update_guard
BEFORE UPDATE OF revision_of_id, project_id ON documents
WHEN NEW.revision_of_id IS NOT NULL AND (
  NEW.revision_of_id = NEW.id OR NOT EXISTS (
    SELECT 1 FROM documents d
    WHERE d.id = NEW.revision_of_id AND d.project_id = NEW.project_id
  ) OR EXISTS (
    WITH RECURSIVE ancestors(id) AS (
      SELECT NEW.revision_of_id
      UNION ALL
      SELECT d.revision_of_id FROM documents d JOIN ancestors a ON d.id = a.id
      WHERE d.revision_of_id IS NOT NULL
    ) SELECT 1 FROM ancestors WHERE id = NEW.id
  )
) BEGIN
  SELECT RAISE(ABORT, 'invalid document revision chain');
END;
)sql";

constexpr auto schema_version_10 = R"sql(
-- Qualificadores não substituem a relação: registram apenas o contexto
-- opcional em que o vínculo é considerado pelo autor.
CREATE TABLE relation_contexts (
  relation_id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  fictional_time_point_id TEXT,
  location_entity_id TEXT,
  cause_entity_id TEXT,
  FOREIGN KEY (relation_id) REFERENCES relations(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (fictional_time_point_id)
    REFERENCES fictional_time_points(id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (location_entity_id) REFERENCES entities(id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (cause_entity_id) REFERENCES entities(id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX relation_contexts_project_time_idx
  ON relation_contexts(project_id, fictional_time_point_id, relation_id);
CREATE INDEX relation_contexts_project_location_idx
  ON relation_contexts(project_id, location_entity_id, relation_id);
CREATE INDEX relation_contexts_project_cause_idx
  ON relation_contexts(project_id, cause_entity_id, relation_id);

-- Todo vínculo pré-existente recebe um contexto vazio para manter uma leitura
-- simples e uma relação um-para-um estável durante as consultas.
INSERT INTO relation_contexts(relation_id, project_id)
  SELECT id, project_id FROM relations;

CREATE TRIGGER relation_context_relation_project_insert_guard
BEFORE INSERT ON relation_contexts
WHEN NOT EXISTS (
  SELECT 1 FROM relations r
  WHERE r.id = NEW.relation_id AND r.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'relation context must belong to the active project');
END;
CREATE TRIGGER relation_context_relation_project_update_guard
BEFORE UPDATE OF relation_id, project_id ON relation_contexts
WHEN NOT EXISTS (
  SELECT 1 FROM relations r
  WHERE r.id = NEW.relation_id AND r.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'relation context must belong to the active project');
END;
CREATE TRIGGER relation_context_time_project_insert_guard
BEFORE INSERT ON relation_contexts
WHEN NEW.fictional_time_point_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM fictional_time_points point
  WHERE point.id = NEW.fictional_time_point_id AND point.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'relation time point must belong to the active project');
END;
CREATE TRIGGER relation_context_time_project_update_guard
BEFORE UPDATE OF fictional_time_point_id, project_id ON relation_contexts
WHEN NEW.fictional_time_point_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM fictional_time_points point
  WHERE point.id = NEW.fictional_time_point_id AND point.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'relation time point must belong to the active project');
END;
CREATE TRIGGER relation_context_location_insert_guard
BEFORE INSERT ON relation_contexts
WHEN NEW.location_entity_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM entities entity
  WHERE entity.id = NEW.location_entity_id AND entity.project_id = NEW.project_id
    AND entity.entity_type_id = '00000000-0000-4000-9000-000000000002'
) BEGIN
  SELECT RAISE(ABORT, 'relation location must be a project location entity');
END;
CREATE TRIGGER relation_context_location_update_guard
BEFORE UPDATE OF location_entity_id, project_id ON relation_contexts
WHEN NEW.location_entity_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM entities entity
  WHERE entity.id = NEW.location_entity_id AND entity.project_id = NEW.project_id
    AND entity.entity_type_id = '00000000-0000-4000-9000-000000000002'
) BEGIN
  SELECT RAISE(ABORT, 'relation location must be a project location entity');
END;
CREATE TRIGGER relation_context_cause_project_insert_guard
BEFORE INSERT ON relation_contexts
WHEN NEW.cause_entity_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM entities entity
  WHERE entity.id = NEW.cause_entity_id AND entity.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'relation cause must belong to the active project');
END;
CREATE TRIGGER relation_context_cause_project_update_guard
BEFORE UPDATE OF cause_entity_id, project_id ON relation_contexts
WHEN NEW.cause_entity_id IS NOT NULL AND NOT EXISTS (
  SELECT 1 FROM entities entity
  WHERE entity.id = NEW.cause_entity_id AND entity.project_id = NEW.project_id
) BEGIN
  SELECT RAISE(ABORT, 'relation cause must belong to the active project');
END;
)sql";

constexpr auto schema_version_11 = R"sql(
CREATE TABLE structural_element_types (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  name TEXT NOT NULL COLLATE NOCASE CHECK (length(name) BETWEEN 1 AND 128),
  is_builtin INTEGER NOT NULL CHECK (is_builtin IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (project_id, name),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);

INSERT INTO structural_element_types
  (id, project_id, name, is_builtin, created_at, updated_at)
SELECT '10000000-0000-4000-9000-000000000001', id, 'Série', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000002', id, 'Saga', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000003', id, 'Parte', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000004', id, 'Ato', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000005', id, 'Volume', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000006', id, 'Arco', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000007', id, 'Capítulo', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000008', id, 'Seção', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '10000000-0000-4000-9000-000000000009', id, 'Cena', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects;

ALTER TABLE editorial_nodes
  ADD COLUMN structural_type_id TEXT NOT NULL DEFAULT '';
ALTER TABLE editorial_nodes
  ADD COLUMN designator TEXT NOT NULL DEFAULT '' CHECK (length(designator) <= 64);
ALTER TABLE editorial_node_trash
  ADD COLUMN structural_type_id TEXT NOT NULL DEFAULT '';
ALTER TABLE editorial_node_trash
  ADD COLUMN designator TEXT NOT NULL DEFAULT '' CHECK (length(designator) <= 64);

INSERT INTO structural_element_types
  (id, project_id, name, is_builtin, created_at, updated_at)
SELECT lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) ||
       '-4' || substr(lower(hex(randomblob(2))), 2) || '-a' ||
       substr(lower(hex(randomblob(2))), 2) || '-' || lower(hex(randomblob(6))),
       source.project_id, source.name, 0,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
FROM (
  SELECT ip.project_id AS project_id, MIN(n.custom_type_name) AS name
  FROM editorial_nodes n
  JOIN works w ON w.id = n.work_id
  JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
  WHERE n.type = 'custom'
  GROUP BY ip.project_id, n.custom_type_name COLLATE NOCASE
) source;

INSERT INTO structural_element_types
  (id, project_id, name, is_builtin, created_at, updated_at)
SELECT lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) ||
       '-4' || substr(lower(hex(randomblob(2))), 2) || '-a' ||
       substr(lower(hex(randomblob(2))), 2) || '-' || lower(hex(randomblob(6))),
       source.project_id, source.name, 0,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
FROM (
  SELECT ip.project_id AS project_id, MIN(n.custom_type_name) AS name
  FROM editorial_node_trash n
  JOIN works w ON w.id = n.work_id
  JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
  WHERE n.type = 'custom'
  GROUP BY ip.project_id, n.custom_type_name COLLATE NOCASE
) source
WHERE NOT EXISTS (
  SELECT 1 FROM structural_element_types t
  WHERE t.project_id = source.project_id AND t.name = source.name
);

UPDATE editorial_nodes SET structural_type_id = CASE type
  WHEN 'series' THEN '10000000-0000-4000-9000-000000000001'
  WHEN 'saga' THEN '10000000-0000-4000-9000-000000000002'
  WHEN 'part' THEN '10000000-0000-4000-9000-000000000003'
  WHEN 'act' THEN '10000000-0000-4000-9000-000000000004'
  WHEN 'volume' THEN '10000000-0000-4000-9000-000000000005'
  WHEN 'arc' THEN '10000000-0000-4000-9000-000000000006'
  WHEN 'chapter' THEN '10000000-0000-4000-9000-000000000007'
  WHEN 'section' THEN '10000000-0000-4000-9000-000000000008'
  WHEN 'scene' THEN '10000000-0000-4000-9000-000000000009'
  ELSE (SELECT t.id FROM structural_element_types t
        JOIN works w ON w.id = editorial_nodes.work_id
        JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
        WHERE t.project_id = ip.project_id
          AND t.name = editorial_nodes.custom_type_name LIMIT 1)
END;

UPDATE editorial_node_trash SET structural_type_id = CASE type
  WHEN 'series' THEN '10000000-0000-4000-9000-000000000001'
  WHEN 'saga' THEN '10000000-0000-4000-9000-000000000002'
  WHEN 'part' THEN '10000000-0000-4000-9000-000000000003'
  WHEN 'act' THEN '10000000-0000-4000-9000-000000000004'
  WHEN 'volume' THEN '10000000-0000-4000-9000-000000000005'
  WHEN 'arc' THEN '10000000-0000-4000-9000-000000000006'
  WHEN 'chapter' THEN '10000000-0000-4000-9000-000000000007'
  WHEN 'section' THEN '10000000-0000-4000-9000-000000000008'
  WHEN 'scene' THEN '10000000-0000-4000-9000-000000000009'
  ELSE (SELECT t.id FROM structural_element_types t
        JOIN works w ON w.id = editorial_node_trash.work_id
        JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id
        WHERE t.project_id = ip.project_id
          AND t.name = editorial_node_trash.custom_type_name LIMIT 1)
END;

WITH numbered AS (
  SELECT id, CAST(ROW_NUMBER() OVER (
    PARTITION BY work_id, COALESCE(parent_id, ''), structural_type_id
    ORDER BY position, id) AS TEXT) AS value
  FROM editorial_nodes
)
UPDATE editorial_nodes
SET designator = (SELECT value FROM numbered WHERE numbered.id = editorial_nodes.id);

WITH numbered AS (
  SELECT operation_id, id, CAST(ROW_NUMBER() OVER (
    PARTITION BY operation_id, work_id, COALESCE(parent_id, ''),
                 structural_type_id
    ORDER BY position, id) AS TEXT) AS value
  FROM editorial_node_trash
)
UPDATE editorial_node_trash
SET designator = (SELECT value FROM numbered
                  WHERE numbered.operation_id = editorial_node_trash.operation_id
                    AND numbered.id = editorial_node_trash.id);

CREATE TRIGGER editorial_nodes_structural_type_insert_guard
BEFORE INSERT ON editorial_nodes
WHEN NOT EXISTS (
  SELECT 1 FROM structural_element_types t
  JOIN intellectual_properties ip ON ip.project_id = t.project_id
  JOIN works w ON w.intellectual_property_id = ip.id
  WHERE t.id = NEW.structural_type_id AND w.id = NEW.work_id
) BEGIN
  SELECT RAISE(ABORT, 'structural type must belong to the active project');
END;

CREATE TRIGGER editorial_nodes_structural_type_update_guard
BEFORE UPDATE OF structural_type_id, work_id ON editorial_nodes
WHEN NOT EXISTS (
  SELECT 1 FROM structural_element_types t
  JOIN intellectual_properties ip ON ip.project_id = t.project_id
  JOIN works w ON w.intellectual_property_id = ip.id
  WHERE t.id = NEW.structural_type_id AND w.id = NEW.work_id
) BEGIN
  SELECT RAISE(ABORT, 'structural type must belong to the active project');
END;

CREATE TRIGGER structural_element_types_builtin_update_guard
BEFORE UPDATE OF name, is_builtin ON structural_element_types
WHEN OLD.is_builtin = 1 BEGIN
  SELECT RAISE(ABORT, 'builtin structural type cannot be changed');
END;

CREATE TRIGGER structural_element_types_delete_guard
BEFORE DELETE ON structural_element_types
WHEN OLD.is_builtin = 1
  OR EXISTS (SELECT 1 FROM editorial_nodes n
             WHERE n.structural_type_id = OLD.id)
  OR EXISTS (SELECT 1 FROM editorial_node_trash n
             WHERE n.structural_type_id = OLD.id) BEGIN
  SELECT RAISE(ABORT, 'structural type is protected or in use');
END;
)sql";

constexpr auto schema_version_14 = R"sql(
CREATE TABLE cartographic_planets (
  id TEXT PRIMARY KEY CHECK(length(id)>0),
  project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(length(name)>0 AND length(name)<=512),
  radius_m INTEGER NOT NULL CHECK(radius_m BETWEEN 1000 AND 1000000000),
  seed INTEGER NOT NULL CHECK(seed BETWEEN 0 AND 2147483647),
  water_percent INTEGER NOT NULL CHECK(water_percent BETWEEN 5 AND 95),
  fragmentation INTEGER NOT NULL CHECK(fragmentation BETWEEN 1 AND 8),
  height_m INTEGER NOT NULL CHECK(height_m BETWEEN 100 AND 30000),
  depth_m INTEGER NOT NULL CHECK(depth_m BETWEEN 100 AND 30000),
  detail INTEGER NOT NULL CHECK(detail BETWEEN 0 AND 3),
  generator INTEGER NOT NULL CHECK(generator=1),
  created_at TEXT NOT NULL CHECK(length(created_at)>0),
  UNIQUE(id,project_id)
);
CREATE INDEX cartographic_planets_project_idx ON cartographic_planets(project_id,name,id);
CREATE TABLE cartographic_chunks (
  planet_id TEXT NOT NULL REFERENCES cartographic_planets(id) ON DELETE CASCADE,
  level INTEGER NOT NULL CHECK(level BETWEEN 0 AND 3),
  x INTEGER NOT NULL CHECK(x>=0 AND x<(2<<level)),
  y INTEGER NOT NULL CHECK(y>=0 AND y<(1<<level)),
  elevation BLOB NOT NULL CHECK(typeof(elevation)='blob' AND length(elevation)=8192),
  PRIMARY KEY(planet_id,level,x,y)
) WITHOUT ROWID;
CREATE TABLE cartographic_positions (
  id INTEGER PRIMARY KEY,
  project_id TEXT NOT NULL,
  planet_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  longitude_e6 INTEGER NOT NULL CHECK(longitude_e6 BETWEEN -180000000 AND 179999999),
  latitude_e6 INTEGER NOT NULL CHECK(latitude_e6 BETWEEN -90000000 AND 90000000),
  approximate INTEGER NOT NULL CHECK(approximate IN (0,1)),
  importance INTEGER NOT NULL CHECK(importance BETWEEN 1 AND 100),
  min_level INTEGER NOT NULL CHECK(min_level BETWEEN 0 AND 6),
  symbol TEXT NOT NULL CHECK(symbol IN ('place','city','mountain')),
  UNIQUE(planet_id,entity_id),
  FOREIGN KEY(planet_id,project_id) REFERENCES cartographic_planets(id,project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY(entity_id,project_id) REFERENCES entities(id,project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX cartographic_positions_entity_idx ON cartographic_positions(project_id,entity_id);
CREATE VIRTUAL TABLE cartographic_position_index USING rtree_i32(id,min_lon,max_lon,min_lat,max_lat);
CREATE TRIGGER cartographic_position_insert AFTER INSERT ON cartographic_positions BEGIN
  INSERT INTO cartographic_position_index VALUES(NEW.id,NEW.longitude_e6,NEW.longitude_e6,NEW.latitude_e6,NEW.latitude_e6);
END;
CREATE TRIGGER cartographic_position_update AFTER UPDATE OF longitude_e6,latitude_e6 ON cartographic_positions BEGIN
  UPDATE cartographic_position_index SET min_lon=NEW.longitude_e6,max_lon=NEW.longitude_e6,
    min_lat=NEW.latitude_e6,max_lat=NEW.latitude_e6 WHERE id=NEW.id;
END;
CREATE TRIGGER cartographic_position_delete AFTER DELETE ON cartographic_positions BEGIN
  DELETE FROM cartographic_position_index WHERE id=OLD.id;
END;
CREATE TRIGGER cartographic_local_insert BEFORE INSERT ON cartographic_positions
WHEN NOT EXISTS(SELECT 1 FROM entities WHERE id=NEW.entity_id
  AND entity_type_id='00000000-0000-4000-9000-000000000002') BEGIN
  SELECT RAISE(ABORT,'cartography requires a Local entity');
END;
CREATE TRIGGER cartographic_local_update BEFORE UPDATE OF entity_id ON cartographic_positions
WHEN NOT EXISTS(SELECT 1 FROM entities WHERE id=NEW.entity_id
  AND entity_type_id='00000000-0000-4000-9000-000000000002') BEGIN
  SELECT RAISE(ABORT,'cartography requires a Local entity');
END;
CREATE TRIGGER cartographic_local_type_guard BEFORE UPDATE OF entity_type_id ON entities
WHEN NEW.entity_type_id<>'00000000-0000-4000-9000-000000000002'
  AND EXISTS(SELECT 1 FROM cartographic_positions WHERE entity_id=OLD.id) BEGIN
  SELECT RAISE(ABORT,'remove cartographic positions before changing the Local type');
END;
)sql";

constexpr auto schema_version_13 = R"sql(
CREATE UNIQUE INDEX document_anchors_identity_idx
  ON document_anchors(id, document_id, project_id);
CREATE TABLE document_text_references (
  id TEXT PRIMARY KEY CHECK (length(id) > 0),
  project_id TEXT NOT NULL,
  source_document_id TEXT NOT NULL,
  target_document_id TEXT NOT NULL,
  target_anchor_id TEXT,
  notes TEXT NOT NULL DEFAULT '' CHECK (length(notes) <= 4096),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  CHECK (source_document_id <> target_document_id),
  CHECK (target_anchor_id IS NULL OR length(target_anchor_id) > 0),
  FOREIGN KEY (source_document_id, project_id) REFERENCES documents(id, project_id)
    ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (target_document_id, project_id) REFERENCES documents(id, project_id)
    ON DELETE NO ACTION DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (target_anchor_id, target_document_id, project_id)
    REFERENCES document_anchors(id, document_id, project_id)
    ON DELETE NO ACTION DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX document_text_references_unique_idx
  ON document_text_references(source_document_id, target_document_id,
                              COALESCE(target_anchor_id, ''));
CREATE INDEX document_text_references_source_idx
  ON document_text_references(project_id, source_document_id, created_at, id);
CREATE INDEX document_text_references_target_idx
  ON document_text_references(project_id, target_document_id, created_at, id);
CREATE INDEX document_text_references_anchor_idx
  ON document_text_references(project_id, target_anchor_id, created_at, id);
)sql";

constexpr auto schema_version_12 = R"sql(
CREATE TABLE structure_models (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  layer TEXT NOT NULL CHECK (layer IN ('editorial', 'narrative')),
  model_key TEXT NOT NULL CHECK (length(model_key) > 0),
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  is_builtin INTEGER NOT NULL DEFAULT 0 CHECK (is_builtin IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (project_id, layer, model_key),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE structure_model_items (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  model_id TEXT NOT NULL,
  parent_id TEXT,
  item_kind TEXT NOT NULL CHECK (
    item_kind IN ('editorial-node', 'narrative-line', 'narrative-unit')
  ),
  type_key TEXT NOT NULL DEFAULT '',
  designator TEXT NOT NULL DEFAULT '' CHECK (length(designator) <= 64),
  title TEXT NOT NULL CHECK (length(title) > 0),
  summary TEXT NOT NULL DEFAULT '',
  purpose TEXT NOT NULL DEFAULT '',
  perspective TEXT NOT NULL DEFAULT '',
  position INTEGER NOT NULL CHECK (position >= 0),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  FOREIGN KEY (model_id, project_id)
    REFERENCES structure_models(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (parent_id, project_id)
    REFERENCES structure_model_items(id, project_id)
    DEFERRABLE INITIALLY DEFERRED,
  CHECK (parent_id IS NULL OR parent_id <> id)
);
CREATE INDEX structure_model_items_model_position_idx
  ON structure_model_items(project_id, model_id, position, id);

CREATE TABLE structure_model_item_links (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  model_id TEXT NOT NULL,
  source_item_id TEXT NOT NULL,
  target_item_id TEXT NOT NULL,
  kind TEXT NOT NULL CHECK (
    kind IN ('membership', 'precedes', 'causes', 'alternative', 'depends')
  ),
  label TEXT NOT NULL DEFAULT '',
  position INTEGER NOT NULL DEFAULT 0 CHECK (position >= 0),
  UNIQUE (source_item_id, target_item_id, kind),
  FOREIGN KEY (model_id, project_id)
    REFERENCES structure_models(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (source_item_id, project_id)
    REFERENCES structure_model_items(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (target_item_id, project_id)
    REFERENCES structure_model_items(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  CHECK (source_item_id <> target_item_id)
);
CREATE INDEX structure_model_item_links_model_idx
  ON structure_model_item_links(project_id, model_id, kind, position, id);

CREATE TABLE editorial_structures (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  work_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  model_id TEXT,
  derived_from_id TEXT,
  creation_kind TEXT NOT NULL CHECK (
    creation_kind IN ('migrated', 'blank', 'instantiated', 'duplicated', 'derived')
  ),
  is_active INTEGER NOT NULL DEFAULT 0 CHECK (is_active IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (work_id) REFERENCES works(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (model_id, project_id)
    REFERENCES structure_models(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (derived_from_id, project_id)
    REFERENCES editorial_structures(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX editorial_structures_one_active_idx
  ON editorial_structures(work_id) WHERE is_active = 1;
CREATE INDEX editorial_structures_work_idx
  ON editorial_structures(project_id, work_id, created_at, id);

CREATE TABLE narrative_structures (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  work_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  model_id TEXT,
  derived_from_id TEXT,
  creation_kind TEXT NOT NULL CHECK (
    creation_kind IN ('migrated', 'blank', 'instantiated', 'duplicated', 'derived')
  ),
  is_active INTEGER NOT NULL DEFAULT 0 CHECK (is_active IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (work_id) REFERENCES works(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (model_id, project_id)
    REFERENCES structure_models(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (derived_from_id, project_id)
    REFERENCES narrative_structures(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX narrative_structures_one_active_idx
  ON narrative_structures(work_id) WHERE is_active = 1;
CREATE INDEX narrative_structures_work_idx
  ON narrative_structures(project_id, work_id, created_at, id);

INSERT INTO structure_models
  (id, project_id, layer, model_key, name, description, is_builtin,
   created_at, updated_at)
SELECT '20000000-0000-4000-9000-000000000001', id, 'editorial',
       'three-acts', 'Três atos',
       'Três atos editoriais sem impor capítulos.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '20000000-0000-4000-9000-000000000002', id, 'editorial',
       'simple-novel', 'Romance simples',
       'Dez capítulos editoriais independentes.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '20000000-0000-4000-9000-000000000003', id, 'editorial',
       'parts-and-chapters', 'Partes e capítulos',
       'Três partes com cinco capítulos em cada uma.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '20000000-0000-4000-9000-000000000004', id, 'narrative',
       'linear', 'Linha narrativa simples',
       'Uma linha principal pronta para receber unidades.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '20000000-0000-4000-9000-000000000005', id, 'narrative',
       'three-movements', 'Três movimentos narrativos',
       'Preparação, confrontação e resolução como unidades editáveis.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects;

INSERT INTO structure_model_items
  (id, project_id, model_id, parent_id, item_kind, type_key, designator,
   title, position, created_at, updated_at)
SELECT '21000000-0000-4000-9000-000000000001', id,
       '20000000-0000-4000-9000-000000000001', NULL,
       'editorial-node', 'act', 'I', 'Sem título', 1000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000002', id,
       '20000000-0000-4000-9000-000000000001', NULL,
       'editorial-node', 'act', 'II', 'Sem título', 2000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000003', id,
       '20000000-0000-4000-9000-000000000001', NULL,
       'editorial-node', 'act', 'III', 'Sem título', 3000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000004', id,
       '20000000-0000-4000-9000-000000000004', NULL,
       'narrative-line', '', '', 'Linha principal', 1000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000005', id,
       '20000000-0000-4000-9000-000000000005', NULL,
       'narrative-line', '', '', 'Linha principal', 1000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000006', id,
       '20000000-0000-4000-9000-000000000005', NULL,
       'narrative-unit', '', '1', 'Preparação', 2000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000007', id,
       '20000000-0000-4000-9000-000000000005', NULL,
       'narrative-unit', '', '2', 'Confrontação', 3000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '21000000-0000-4000-9000-000000000008', id,
       '20000000-0000-4000-9000-000000000005', NULL,
       'narrative-unit', '', '3', 'Resolução', 4000,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects;

INSERT INTO editorial_structures
  (id, project_id, work_id, name, description, creation_kind, is_active,
   created_at, updated_at)
SELECT lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) ||
       '-4' || substr(lower(hex(randomblob(2))), 2) || '-a' ||
       substr(lower(hex(randomblob(2))), 2) || '-' || lower(hex(randomblob(6))),
       ip.project_id, w.id, 'Estrutura editorial atual',
       'Estrutura preservada pela migração do esquema v11.', 'migrated', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
FROM works w JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id;

INSERT INTO narrative_structures
  (id, project_id, work_id, name, description, creation_kind, is_active,
   created_at, updated_at)
SELECT lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) ||
       '-4' || substr(lower(hex(randomblob(2))), 2) || '-a' ||
       substr(lower(hex(randomblob(2))), 2) || '-' || lower(hex(randomblob(6))),
       ip.project_id, w.id, 'Narrativa principal',
       'Estrutura narrativa inicial, independente da árvore editorial.',
       'blank', 1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
FROM works w JOIN intellectual_properties ip ON ip.id = w.intellectual_property_id;

CREATE TRIGGER works_default_structures
AFTER INSERT ON works BEGIN
  INSERT INTO editorial_structures
    (id, project_id, work_id, name, description, creation_kind, is_active,
     created_at, updated_at)
  SELECT lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) ||
         '-4' || substr(lower(hex(randomblob(2))), 2) || '-a' ||
         substr(lower(hex(randomblob(2))), 2) || '-' || lower(hex(randomblob(6))),
         ip.project_id, NEW.id, 'Estrutura editorial principal',
         'Estrutura editorial inicial da Obra.', 'blank', 1,
         strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
         strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
  FROM intellectual_properties ip WHERE ip.id = NEW.intellectual_property_id;
  INSERT INTO narrative_structures
    (id, project_id, work_id, name, description, creation_kind, is_active,
     created_at, updated_at)
  SELECT lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) ||
         '-4' || substr(lower(hex(randomblob(2))), 2) || '-a' ||
         substr(lower(hex(randomblob(2))), 2) || '-' || lower(hex(randomblob(6))),
         ip.project_id, NEW.id, 'Narrativa principal',
         'Estrutura narrativa inicial, independente da árvore editorial.',
         'blank', 1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
         strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
  FROM intellectual_properties ip WHERE ip.id = NEW.intellectual_property_id;
END;

ALTER TABLE editorial_nodes ADD COLUMN structure_id TEXT NOT NULL DEFAULT '';
ALTER TABLE editorial_node_trash ADD COLUMN structure_id TEXT NOT NULL DEFAULT '';
UPDATE editorial_nodes SET structure_id = (
  SELECT s.id FROM editorial_structures s
  WHERE s.work_id = editorial_nodes.work_id AND s.is_active = 1
);
UPDATE editorial_node_trash SET structure_id = (
  SELECT s.id FROM editorial_structures s
  WHERE s.work_id = editorial_node_trash.work_id AND s.is_active = 1
);
CREATE INDEX editorial_nodes_structure_parent_position_idx
  ON editorial_nodes(structure_id, parent_id, position, id);

CREATE TRIGGER editorial_nodes_structure_insert_guard
BEFORE INSERT ON editorial_nodes
WHEN (length(NEW.structure_id) > 0 AND NOT EXISTS (
  SELECT 1 FROM editorial_structures s
  WHERE s.id = NEW.structure_id AND s.work_id = NEW.work_id
)) OR (length(NEW.structure_id) > 0 AND NEW.parent_id IS NOT NULL
       AND EXISTS (SELECT 1 FROM editorial_nodes p WHERE p.id = NEW.parent_id)
       AND NOT EXISTS (
  SELECT 1 FROM editorial_nodes p
  WHERE p.id = NEW.parent_id AND p.structure_id = NEW.structure_id
)) BEGIN
  SELECT RAISE(ABORT, 'editorial node must belong to one structure and parent');
END;
CREATE TRIGGER editorial_nodes_structure_default
AFTER INSERT ON editorial_nodes WHEN length(NEW.structure_id) = 0 BEGIN
  UPDATE editorial_nodes SET structure_id = (
    SELECT s.id FROM editorial_structures s
    WHERE s.work_id = NEW.work_id AND s.is_active = 1
  ) WHERE id = NEW.id;
END;
CREATE TRIGGER editorial_nodes_structure_update_guard
BEFORE UPDATE OF structure_id, work_id, parent_id ON editorial_nodes
WHEN NOT EXISTS (
  SELECT 1 FROM editorial_structures s
  WHERE s.id = NEW.structure_id AND s.work_id = NEW.work_id
) OR (NEW.parent_id IS NOT NULL
      AND EXISTS (SELECT 1 FROM editorial_nodes p WHERE p.id = NEW.parent_id)
      AND NOT EXISTS (
  SELECT 1 FROM editorial_nodes p
  WHERE p.id = NEW.parent_id AND p.structure_id = NEW.structure_id
)) BEGIN
  SELECT RAISE(ABORT, 'editorial node must belong to one structure and parent');
END;

CREATE TABLE narrative_lines (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  structure_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  position INTEGER NOT NULL CHECK (position >= 0),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (structure_id, name),
  FOREIGN KEY (structure_id, project_id)
    REFERENCES narrative_structures(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX narrative_lines_structure_position_idx
  ON narrative_lines(project_id, structure_id, position, id);

CREATE TABLE narrative_units (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  structure_id TEXT NOT NULL,
  designator TEXT NOT NULL DEFAULT '' CHECK (length(designator) <= 64),
  title TEXT NOT NULL CHECK (length(title) > 0),
  summary TEXT NOT NULL DEFAULT '',
  purpose TEXT NOT NULL DEFAULT '',
  perspective TEXT NOT NULL DEFAULT '',
  position INTEGER NOT NULL CHECK (position >= 0),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  FOREIGN KEY (structure_id, project_id)
    REFERENCES narrative_structures(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX narrative_units_structure_position_idx
  ON narrative_units(project_id, structure_id, position, id);

CREATE TABLE narrative_unit_lines (
  project_id TEXT NOT NULL,
  unit_id TEXT NOT NULL,
  line_id TEXT NOT NULL,
  position INTEGER NOT NULL CHECK (position >= 0),
  PRIMARY KEY (unit_id, line_id),
  FOREIGN KEY (unit_id, project_id)
    REFERENCES narrative_units(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (line_id, project_id)
    REFERENCES narrative_lines(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX narrative_unit_lines_line_position_idx
  ON narrative_unit_lines(project_id, line_id, position, unit_id);

CREATE TABLE narrative_unit_entities (
  project_id TEXT NOT NULL,
  unit_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  role TEXT NOT NULL DEFAULT '',
  PRIMARY KEY (unit_id, entity_id, role),
  FOREIGN KEY (unit_id, project_id)
    REFERENCES narrative_units(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE narrative_links (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  structure_id TEXT NOT NULL,
  source_unit_id TEXT NOT NULL,
  target_unit_id TEXT NOT NULL,
  kind TEXT NOT NULL CHECK (
    kind IN ('precedes', 'causes', 'alternative', 'depends')
  ),
  label TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  CHECK (source_unit_id <> target_unit_id),
  UNIQUE (source_unit_id, target_unit_id, kind),
  FOREIGN KEY (structure_id, project_id)
    REFERENCES narrative_structures(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (source_unit_id, project_id)
    REFERENCES narrative_units(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (target_unit_id, project_id)
    REFERENCES narrative_units(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX narrative_links_structure_idx
  ON narrative_links(project_id, structure_id, kind, source_unit_id);

CREATE TRIGGER narrative_unit_lines_structure_insert_guard
BEFORE INSERT ON narrative_unit_lines
WHEN (SELECT structure_id FROM narrative_units WHERE id = NEW.unit_id)
     <> (SELECT structure_id FROM narrative_lines WHERE id = NEW.line_id)
BEGIN
  SELECT RAISE(ABORT, 'narrative unit and line must share a structure');
END;
CREATE TRIGGER narrative_unit_lines_structure_update_guard
BEFORE UPDATE OF unit_id, line_id ON narrative_unit_lines
WHEN (SELECT structure_id FROM narrative_units WHERE id = NEW.unit_id)
     <> (SELECT structure_id FROM narrative_lines WHERE id = NEW.line_id)
BEGIN
  SELECT RAISE(ABORT, 'narrative unit and line must share a structure');
END;
CREATE TRIGGER narrative_links_structure_insert_guard
BEFORE INSERT ON narrative_links
WHEN (SELECT structure_id FROM narrative_units WHERE id = NEW.source_unit_id)
       <> NEW.structure_id
  OR (SELECT structure_id FROM narrative_units WHERE id = NEW.target_unit_id)
       <> NEW.structure_id
BEGIN
  SELECT RAISE(ABORT, 'narrative link units must share its structure');
END;
CREATE TRIGGER narrative_links_structure_update_guard
BEFORE UPDATE OF structure_id, source_unit_id, target_unit_id ON narrative_links
WHEN (SELECT structure_id FROM narrative_units WHERE id = NEW.source_unit_id)
       <> NEW.structure_id
  OR (SELECT structure_id FROM narrative_units WHERE id = NEW.target_unit_id)
       <> NEW.structure_id
BEGIN
  SELECT RAISE(ABORT, 'narrative link units must share its structure');
END;

CREATE TABLE narrative_roles (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  is_builtin INTEGER NOT NULL DEFAULT 0 CHECK (is_builtin IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (project_id, name),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE narrative_role_assignments (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  role_id TEXT NOT NULL,
  entity_id TEXT NOT NULL,
  work_id TEXT NOT NULL,
  structure_id TEXT,
  unit_id TEXT,
  notes TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (role_id, entity_id, work_id, structure_id, unit_id),
  CHECK (unit_id IS NULL OR structure_id IS NOT NULL),
  FOREIGN KEY (role_id, project_id)
    REFERENCES narrative_roles(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (work_id) REFERENCES works(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (structure_id, project_id)
    REFERENCES narrative_structures(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (unit_id, project_id)
    REFERENCES narrative_units(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX narrative_role_assignments_scope_idx
  ON narrative_role_assignments(
    role_id, entity_id, work_id, COALESCE(structure_id, ''),
    COALESCE(unit_id, '')
  );
CREATE TRIGGER narrative_role_assignments_scope_insert_guard
BEFORE INSERT ON narrative_role_assignments
WHEN (NEW.structure_id IS NOT NULL AND NOT EXISTS (
       SELECT 1 FROM narrative_structures s
       WHERE s.id = NEW.structure_id AND s.work_id = NEW.work_id
     )) OR (NEW.unit_id IS NOT NULL AND NOT EXISTS (
       SELECT 1 FROM narrative_units u
       WHERE u.id = NEW.unit_id AND u.structure_id = NEW.structure_id
     ))
BEGIN
  SELECT RAISE(ABORT, 'narrative role scope is inconsistent');
END;
CREATE TRIGGER narrative_role_assignments_scope_update_guard
BEFORE UPDATE OF work_id, structure_id, unit_id ON narrative_role_assignments
WHEN (NEW.structure_id IS NOT NULL AND NOT EXISTS (
       SELECT 1 FROM narrative_structures s
       WHERE s.id = NEW.structure_id AND s.work_id = NEW.work_id
     )) OR (NEW.unit_id IS NOT NULL AND NOT EXISTS (
       SELECT 1 FROM narrative_units u
       WHERE u.id = NEW.unit_id AND u.structure_id = NEW.structure_id
     ))
BEGIN
  SELECT RAISE(ABORT, 'narrative role scope is inconsistent');
END;

INSERT INTO narrative_roles
  (id, project_id, name, description, is_builtin, created_at, updated_at)
SELECT '22000000-0000-4000-9000-000000000001', id, 'Protagonista',
       'Papel contextual de protagonismo.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '22000000-0000-4000-9000-000000000002', id, 'Antagonista',
       'Papel contextual de oposição.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '22000000-0000-4000-9000-000000000003', id, 'Testemunha',
       'Papel contextual de testemunho.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '22000000-0000-4000-9000-000000000004', id, 'Mentor',
       'Papel contextual de mentoria.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects
UNION ALL SELECT '22000000-0000-4000-9000-000000000005', id, 'Ponto de vista (PoV)',
       'Papel contextual de ponto de vista; não substitui toda perspectiva.', 1,
       strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') FROM projects;

CREATE TRIGGER structure_models_builtin_update_guard
BEFORE UPDATE OF layer, model_key, name, is_builtin ON structure_models
WHEN OLD.is_builtin = 1 BEGIN
  SELECT RAISE(ABORT, 'builtin structure model cannot be changed');
END;
CREATE TRIGGER structure_models_builtin_delete_guard
BEFORE DELETE ON structure_models WHEN OLD.is_builtin = 1 BEGIN
  SELECT RAISE(ABORT, 'builtin structure model cannot be deleted');
END;
CREATE TRIGGER narrative_roles_builtin_update_guard
BEFORE UPDATE OF name, is_builtin ON narrative_roles
WHEN OLD.is_builtin = 1 BEGIN
  SELECT RAISE(ABORT, 'builtin narrative role cannot be changed');
END;
CREATE TRIGGER narrative_roles_builtin_delete_guard
BEFORE DELETE ON narrative_roles WHEN OLD.is_builtin = 1 BEGIN
  SELECT RAISE(ABORT, 'builtin narrative role cannot be deleted');
END;
)sql";

constexpr auto schema_version_1 = R"sql(
CREATE TABLE projects (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL CHECK (length(name) > 0),
  format_version INTEGER NOT NULL CHECK (format_version > 0),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0)
);

CREATE TABLE intellectual_properties (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  title TEXT NOT NULL CHECK (length(title) > 0),
  subtitle TEXT NOT NULL DEFAULT '',
  description TEXT NOT NULL DEFAULT '',
  cover_path TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
);

CREATE INDEX intellectual_properties_project_idx
  ON intellectual_properties(project_id);

CREATE TABLE works (
  id TEXT PRIMARY KEY,
  intellectual_property_id TEXT NOT NULL,
  title TEXT NOT NULL CHECK (length(title) > 0),
  subtitle TEXT NOT NULL DEFAULT '',
  synopsis TEXT NOT NULL DEFAULT '',
  language TEXT NOT NULL CHECK (length(language) > 0),
  status TEXT NOT NULL CHECK (length(status) > 0),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  FOREIGN KEY (intellectual_property_id)
    REFERENCES intellectual_properties(id) ON DELETE RESTRICT
);

CREATE INDEX works_intellectual_property_idx
  ON works(intellectual_property_id);

CREATE TABLE editorial_nodes (
  id TEXT PRIMARY KEY,
  work_id TEXT NOT NULL,
  parent_id TEXT,
  type TEXT NOT NULL CHECK (
    type IN ('saga', 'series', 'volume', 'part', 'act', 'arc',
             'chapter', 'section', 'scene', 'custom')
  ),
  title TEXT NOT NULL CHECK (length(title) > 0),
  subtitle TEXT NOT NULL DEFAULT '',
  synopsis TEXT NOT NULL DEFAULT '',
  custom_type_name TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL CHECK (length(status) > 0),
  position INTEGER NOT NULL CHECK (position >= 0),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, work_id),
  CHECK (type <> 'custom' OR length(custom_type_name) > 0),
  CHECK (parent_id IS NULL OR parent_id <> id),
  FOREIGN KEY (work_id) REFERENCES works(id) ON DELETE RESTRICT,
  FOREIGN KEY (parent_id, work_id)
    REFERENCES editorial_nodes(id, work_id)
    DEFERRABLE INITIALLY DEFERRED
);

CREATE INDEX editorial_nodes_work_parent_position_idx
  ON editorial_nodes(work_id, parent_id, position);
)sql";

constexpr auto schema_version_2 = R"sql(
CREATE TABLE editorial_trash_operations (
  sequence INTEGER PRIMARY KEY AUTOINCREMENT,
  id TEXT NOT NULL UNIQUE,
  created_at TEXT NOT NULL CHECK (length(created_at) > 0)
);

CREATE TABLE editorial_node_trash (
  operation_id TEXT NOT NULL,
  id TEXT NOT NULL,
  work_id TEXT NOT NULL,
  parent_id TEXT,
  type TEXT NOT NULL,
  title TEXT NOT NULL,
  subtitle TEXT NOT NULL,
  synopsis TEXT NOT NULL,
  custom_type_name TEXT NOT NULL,
  status TEXT NOT NULL,
  position INTEGER NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  PRIMARY KEY (operation_id, id),
  FOREIGN KEY (operation_id) REFERENCES editorial_trash_operations(id)
    ON DELETE CASCADE
);

CREATE INDEX editorial_node_trash_operation_idx
  ON editorial_node_trash(operation_id);
)sql";

constexpr auto schema_version_3 = R"sql(
CREATE TABLE entity_types (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  key TEXT NOT NULL CHECK (
    length(key) BETWEEN 1 AND 64
    AND key GLOB '[a-z]*'
    AND key NOT GLOB '*[^a-z0-9-]*'
  ),
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  is_builtin INTEGER NOT NULL DEFAULT 0 CHECK (is_builtin IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (project_id, key),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE entities (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  entity_type_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) > 0),
  summary TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (entity_type_id, project_id)
    REFERENCES entity_types(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);

CREATE INDEX entities_project_type_name_idx
  ON entities(project_id, entity_type_id, name COLLATE NOCASE, id);
CREATE INDEX entities_project_name_idx
  ON entities(project_id, name COLLATE NOCASE, id);

CREATE TRIGGER entity_types_key_immutable
BEFORE UPDATE OF key ON entity_types
WHEN NEW.key <> OLD.key
BEGIN
  SELECT RAISE(ABORT, 'entity type key is immutable');
END;

CREATE TRIGGER entity_types_builtin_update_guard
BEFORE UPDATE OF name, description, is_builtin ON entity_types
WHEN OLD.is_builtin = 1
BEGIN
  SELECT RAISE(ABORT, 'builtin entity type is immutable');
END;

CREATE TRIGGER entity_types_builtin_delete_guard
BEFORE DELETE ON entity_types
WHEN OLD.is_builtin = 1
BEGIN
  SELECT RAISE(ABORT, 'builtin entity type cannot be deleted');
END;

CREATE TABLE relation_types (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  key TEXT NOT NULL CHECK (
    length(key) BETWEEN 1 AND 64
    AND key GLOB '[a-z]*'
    AND key NOT GLOB '*[^a-z0-9-]*'
  ),
  name TEXT NOT NULL CHECK (length(name) > 0),
  inverse_name TEXT NOT NULL DEFAULT '',
  description TEXT NOT NULL DEFAULT '',
  directionality TEXT NOT NULL CHECK (
    directionality IN ('directed', 'symmetric')
  ),
  is_builtin INTEGER NOT NULL DEFAULT 0 CHECK (is_builtin IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (project_id, key),
  CHECK (directionality = 'symmetric' OR length(inverse_name) > 0),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE relations (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  relation_type_id TEXT NOT NULL,
  source_entity_id TEXT NOT NULL,
  target_entity_id TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (relation_type_id, source_entity_id, target_entity_id),
  CHECK (source_entity_id <> target_entity_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (relation_type_id, project_id)
    REFERENCES relation_types(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (source_entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (target_entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);

CREATE INDEX relations_project_source_idx
  ON relations(project_id, source_entity_id);
CREATE INDEX relations_project_target_idx
  ON relations(project_id, target_entity_id);

CREATE TRIGGER relation_types_key_immutable
BEFORE UPDATE OF key ON relation_types
WHEN NEW.key <> OLD.key
BEGIN
  SELECT RAISE(ABORT, 'relation type key is immutable');
END;

CREATE TRIGGER relation_types_directionality_immutable
BEFORE UPDATE OF directionality ON relation_types
WHEN NEW.directionality <> OLD.directionality
BEGIN
  SELECT RAISE(ABORT, 'relation directionality is immutable');
END;

CREATE TRIGGER relations_symmetric_insert_guard
BEFORE INSERT ON relations
WHEN (SELECT directionality FROM relation_types WHERE id = NEW.relation_type_id)
       = 'symmetric'
 AND NEW.source_entity_id > NEW.target_entity_id
BEGIN
  SELECT RAISE(ABORT, 'symmetric relation endpoints are not canonical');
END;

CREATE TRIGGER relations_symmetric_update_guard
BEFORE UPDATE OF relation_type_id, source_entity_id, target_entity_id ON relations
WHEN (SELECT directionality FROM relation_types WHERE id = NEW.relation_type_id)
       = 'symmetric'
 AND NEW.source_entity_id > NEW.target_entity_id
BEGIN
  SELECT RAISE(ABORT, 'symmetric relation endpoints are not canonical');
END;

CREATE TABLE change_log (
  sequence INTEGER PRIMARY KEY AUTOINCREMENT,
  id TEXT NOT NULL UNIQUE,
  project_id TEXT NOT NULL,
  command_name TEXT NOT NULL CHECK (length(command_name) > 0),
  system_created_at TEXT NOT NULL CHECK (length(system_created_at) > 0),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);

CREATE INDEX change_log_project_sequence_idx
  ON change_log(project_id, sequence DESC);
)sql";

constexpr auto schema_version_4 = R"sql(
CREATE TABLE fictional_time_axes (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  name TEXT NOT NULL CHECK (length(name) > 0),
  description TEXT NOT NULL DEFAULT '',
  is_default INTEGER NOT NULL DEFAULT 0 CHECK (is_default IN (0, 1)),
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (project_id, name),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX fictional_time_axes_one_default_idx
  ON fictional_time_axes(project_id) WHERE is_default = 1;
CREATE TRIGGER fictional_time_axes_default_delete_guard
BEFORE DELETE ON fictional_time_axes WHEN OLD.is_default = 1 BEGIN
  SELECT RAISE(ABORT, 'default fictional time axis cannot be deleted');
END;
CREATE TRIGGER fictional_time_axes_default_update_guard
BEFORE UPDATE OF is_default ON fictional_time_axes
WHEN NEW.is_default <> OLD.is_default BEGIN
  SELECT RAISE(ABORT, 'default fictional time axis marker is immutable');
END;

CREATE TABLE fictional_time_points (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  axis_id TEXT NOT NULL,
  ordinal INTEGER NOT NULL,
  label TEXT NOT NULL CHECK (length(label) > 0),
  description TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (axis_id, ordinal),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (axis_id, project_id)
    REFERENCES fictional_time_axes(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX fictional_time_points_axis_ordinal_idx
  ON fictional_time_points(project_id, axis_id, ordinal, id);

CREATE TABLE event_occurrences (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  event_entity_id TEXT NOT NULL,
  time_point_id TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (id, project_id),
  UNIQUE (event_entity_id),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (event_entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (time_point_id, project_id)
    REFERENCES fictional_time_points(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX event_occurrences_time_point_idx
  ON event_occurrences(project_id, time_point_id, event_entity_id);
CREATE TRIGGER event_occurrences_type_insert_guard
BEFORE INSERT ON event_occurrences
WHEN (SELECT entity_type_id FROM entities WHERE id = NEW.event_entity_id)
       <> '00000000-0000-4000-9000-000000000003' BEGIN
  SELECT RAISE(ABORT, 'event occurrence requires an event entity');
END;
CREATE TRIGGER event_occurrences_type_update_guard
BEFORE UPDATE OF event_entity_id ON event_occurrences
WHEN (SELECT entity_type_id FROM entities WHERE id = NEW.event_entity_id)
       <> '00000000-0000-4000-9000-000000000003' BEGIN
  SELECT RAISE(ABORT, 'event occurrence requires an event entity');
END;

CREATE TABLE event_participations (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL,
  event_occurrence_id TEXT NOT NULL,
  participant_entity_id TEXT NOT NULL,
  role TEXT NOT NULL CHECK (length(role) > 0),
  notes TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL CHECK (length(created_at) > 0),
  updated_at TEXT NOT NULL CHECK (length(updated_at) > 0),
  UNIQUE (event_occurrence_id, participant_entity_id, role),
  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (event_occurrence_id, project_id)
    REFERENCES event_occurrences(id, project_id) ON DELETE CASCADE
    DEFERRABLE INITIALLY DEFERRED,
  FOREIGN KEY (participant_entity_id, project_id)
    REFERENCES entities(id, project_id) ON DELETE RESTRICT
    DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX event_participations_occurrence_idx
  ON event_participations(project_id, event_occurrence_id, role, id);
CREATE INDEX event_participations_entity_idx
  ON event_participations(project_id, participant_entity_id);
CREATE TRIGGER event_participations_type_insert_guard
BEFORE INSERT ON event_participations
WHEN (SELECT entity_type_id FROM entities WHERE id = NEW.participant_entity_id)
       = '00000000-0000-4000-9000-000000000003' BEGIN
  SELECT RAISE(ABORT, 'event entity cannot be an event participant');
END;
CREATE TRIGGER event_participations_type_update_guard
BEFORE UPDATE OF participant_entity_id ON event_participations
WHEN (SELECT entity_type_id FROM entities WHERE id = NEW.participant_entity_id)
       = '00000000-0000-4000-9000-000000000003' BEGIN
  SELECT RAISE(ABORT, 'event entity cannot be an event participant');
END;
CREATE TRIGGER entities_occurrence_type_update_guard
BEFORE UPDATE OF entity_type_id ON entities
WHEN EXISTS (SELECT 1 FROM event_occurrences WHERE event_entity_id = OLD.id)
 AND NEW.entity_type_id <> '00000000-0000-4000-9000-000000000003' BEGIN
  SELECT RAISE(ABORT, 'placed event must remain an event entity');
END;
CREATE TRIGGER entities_participation_type_update_guard
BEFORE UPDATE OF entity_type_id ON entities
WHEN EXISTS (
       SELECT 1 FROM event_participations WHERE participant_entity_id = OLD.id)
 AND NEW.entity_type_id = '00000000-0000-4000-9000-000000000003' BEGIN
  SELECT RAISE(ABORT, 'event participant cannot become an event entity');
END;
)sql";

} // namespace

void SchemaMigrator::migrate(SqliteDatabase &database) const {
  SqliteTransaction transaction(database);
  database.execute(create_migration_table);

  const int version = current_version(database);
  if (version > current_database_schema_version) {
    throw std::runtime_error(
        "O banco do projeto foi criado por uma versão mais nova do INDE");
  }

  if (version < 1) {
    apply_version_1(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{1});
    record.run();
  }

  if (version < 2) {
    apply_version_2(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{2});
    record.run();
  }

  if (version < 3) {
    apply_version_3(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{3});
    record.run();
  }

  if (version < 4) {
    apply_version_4(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{4});
    record.run();
  }

  if (version < 5) {
    apply_version_5(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{5});
    record.run();
  }

  if (version < 6) {
    apply_version_6(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{6});
    record.run();
  }

  if (version < 7) {
    apply_version_7(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{7});
    record.run();
  }

  if (version < 8) {
    apply_version_8(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{8});
    record.run();
  }

  if (version < 9) {
    apply_version_9(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{9});
    record.run();
  }

  if (version < 10) {
    apply_version_10(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{10});
    record.run();
  }

  if (version < 11) {
    apply_version_11(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{11});
    record.run();
  }

  if (version < 12) {
    apply_version_12(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{12});
    record.run();
  }

  if (version < 13) {
    apply_version_13(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (13, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.run();
  }

  if (version < 14) {
    apply_version_14(database);
    database.execute("INSERT INTO schema_migrations(version, applied_at) "
                     "VALUES (14, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
  }

  if (version < 15) {
    apply_version_15(database);
    database.execute("INSERT INTO schema_migrations(version, applied_at) "
                     "VALUES (15, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
  }

  transaction.commit();
}

int SchemaMigrator::current_version(SqliteDatabase &database) const {
  auto exists =
      database.prepare("SELECT count(*) FROM sqlite_master "
                       "WHERE type = 'table' AND name = 'schema_migrations'");
  if (!exists.step() || exists.column_integer(0) == 0) {
    return 0;
  }

  return static_cast<int>(database.query_integer(
      "SELECT COALESCE(MAX(version), 0) FROM schema_migrations"));
}

void SchemaMigrator::apply_version_1(SqliteDatabase &database) {
  database.execute(schema_version_1);
}

void SchemaMigrator::apply_version_2(SqliteDatabase &database) {
  database.execute(schema_version_2);
}

void SchemaMigrator::apply_version_3(SqliteDatabase &database) {
  database.execute(schema_version_3);
}

void SchemaMigrator::apply_version_4(SqliteDatabase &database) {
  database.execute(schema_version_4);
}

void SchemaMigrator::apply_version_5(SqliteDatabase &database) {
  database.execute(schema_version_5);
}

void SchemaMigrator::apply_version_6(SqliteDatabase &database) {
  database.execute(schema_version_6);
}

void SchemaMigrator::apply_version_7(SqliteDatabase &database) {
  database.execute(schema_version_7);
}

void SchemaMigrator::apply_version_8(SqliteDatabase &database) {
  database.execute(schema_version_8);
}

void SchemaMigrator::apply_version_9(SqliteDatabase &database) {
  database.execute(schema_version_9);
}

void SchemaMigrator::apply_version_10(SqliteDatabase &database) {
  database.execute(schema_version_10);
}

void SchemaMigrator::apply_version_11(SqliteDatabase &database) {
  database.execute(schema_version_11);
}

void SchemaMigrator::apply_version_12(SqliteDatabase &database) {
  database.execute(schema_version_12);
}

void SchemaMigrator::apply_version_13(SqliteDatabase &database) {
  database.execute(schema_version_13);
}

void SchemaMigrator::apply_version_14(SqliteDatabase &database) {
  database.execute(schema_version_14);
}

void SchemaMigrator::apply_version_15(SqliteDatabase &database) {
  database.execute(schema_version_15);
}

} // namespace inde::persistence
