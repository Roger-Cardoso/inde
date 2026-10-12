#pragma once

namespace inde::persistence::schema_sql {

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

} // namespace inde::persistence::schema_sql
