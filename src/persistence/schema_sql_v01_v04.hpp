#pragma once

namespace inde::persistence::schema_sql {

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

} // namespace inde::persistence::schema_sql
