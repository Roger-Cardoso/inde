#pragma once

namespace inde::persistence::schema_sql {

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

} // namespace inde::persistence::schema_sql
