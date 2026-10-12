#pragma once
namespace inde::persistence::schema_sql {
// Rebuild the parent and BOTH referencing tables in the same transaction.
// Temporary copies have no FKs; dropping the parent cannot cascade away rows.
inline constexpr auto schema_version_18=R"sql(
CREATE TEMP TABLE semantic_annotations_v18_backup AS SELECT * FROM semantic_annotations;
CREATE TEMP TABLE semantic_entities_v18_backup AS SELECT * FROM semantic_entity_bindings;
CREATE TEMP TABLE semantic_relations_v18_backup AS SELECT * FROM semantic_annotation_relations;
DROP TABLE semantic_annotation_relations;
DROP TABLE semantic_entity_bindings;
DROP TABLE semantic_annotations;
CREATE TABLE semantic_annotations (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL,
 anchor_id TEXT, type_key TEXT NOT NULL, type_version INTEGER NOT NULL CHECK(type_version>0),
 status TEXT NOT NULL CHECK(status IN ('active','archived','incomplete')), category TEXT NOT NULL DEFAULT '',
 scope TEXT NOT NULL DEFAULT 'interval' CHECK(scope IN ('interval','document')),
 CHECK((scope='interval' AND anchor_id IS NOT NULL AND length(anchor_id)>0) OR
       (scope='document' AND anchor_id IS NULL AND type_key='inde.semantic.perspective' AND type_version=1)),
 UNIQUE(id,project_id), UNIQUE(id,document_id,project_id),
 FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(anchor_id,document_id,project_id) REFERENCES document_anchors(id,document_id,project_id) DEFERRABLE INITIALLY DEFERRED
);
INSERT INTO semantic_annotations SELECT *, 'interval' FROM semantic_annotations_v18_backup;
CREATE INDEX semantic_annotations_scope ON semantic_annotations(project_id,type_key,status,document_id,id);
CREATE INDEX semantic_annotations_category ON semantic_annotations(project_id,category,document_id,id);
CREATE UNIQUE INDEX semantic_document_perspective ON semantic_annotations(document_id) WHERE scope='document' AND status!='archived';
CREATE TABLE semantic_entity_bindings (
 project_id TEXT NOT NULL, document_id TEXT NOT NULL, annotation_id TEXT NOT NULL,
 field_key TEXT NOT NULL, ordinal INTEGER NOT NULL, entity_id TEXT NOT NULL,
 PRIMARY KEY(annotation_id,field_key,ordinal),
 FOREIGN KEY(annotation_id,document_id,project_id) REFERENCES semantic_annotations(id,document_id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(entity_id,project_id) REFERENCES entities(id,project_id) DEFERRABLE INITIALLY DEFERRED
);
INSERT INTO semantic_entity_bindings SELECT * FROM semantic_entities_v18_backup;
CREATE INDEX semantic_entity_uses ON semantic_entity_bindings(project_id,entity_id,annotation_id);
CREATE TABLE semantic_annotation_relations (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL,
 source_id TEXT NOT NULL, target_id TEXT NOT NULL, type_key TEXT NOT NULL,
 FOREIGN KEY(source_id,document_id,project_id) REFERENCES semantic_annotations(id,document_id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(target_id,project_id) REFERENCES semantic_annotations(id,project_id) DEFERRABLE INITIALLY DEFERRED,
 CHECK(source_id!=target_id)
);
INSERT INTO semantic_annotation_relations SELECT * FROM semantic_relations_v18_backup;
CREATE INDEX semantic_relation_incoming ON semantic_annotation_relations(project_id,target_id,source_id);
DROP TABLE semantic_relations_v18_backup;
DROP TABLE semantic_entities_v18_backup;
DROP TABLE semantic_annotations_v18_backup;
CREATE UNIQUE INDEX fictional_time_point_axis_identity ON fictional_time_points(id,axis_id,project_id);
CREATE TABLE semantic_fictional_time_bindings (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL, annotation_id TEXT NOT NULL,
 fictional_axis_id TEXT NOT NULL, fictional_start_point_id TEXT NOT NULL, fictional_end_point_id TEXT,
 FOREIGN KEY(annotation_id,document_id,project_id) REFERENCES semantic_annotations(id,document_id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(fictional_axis_id,project_id) REFERENCES fictional_time_axes(id,project_id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(fictional_start_point_id,fictional_axis_id,project_id) REFERENCES fictional_time_points(id,axis_id,project_id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(fictional_end_point_id,fictional_axis_id,project_id) REFERENCES fictional_time_points(id,axis_id,project_id) DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX semantic_fictional_time_uses ON semantic_fictional_time_bindings(project_id,fictional_axis_id,annotation_id);
CREATE UNIQUE INDEX semantic_fictional_time_unique ON semantic_fictional_time_bindings(annotation_id,fictional_axis_id,fictional_start_point_id,COALESCE(fictional_end_point_id,''));
CREATE TRIGGER semantic_fictional_time_order_insert BEFORE INSERT ON semantic_fictional_time_bindings
WHEN NEW.fictional_end_point_id IS NOT NULL BEGIN
 SELECT RAISE(ABORT,'semantic fictional interval is inverted') WHERE
 (SELECT ordinal FROM fictional_time_points WHERE id=NEW.fictional_start_point_id) >
 (SELECT ordinal FROM fictional_time_points WHERE id=NEW.fictional_end_point_id);
END;
CREATE TRIGGER semantic_fictional_time_order_update BEFORE UPDATE OF fictional_start_point_id,fictional_end_point_id ON semantic_fictional_time_bindings
WHEN NEW.fictional_end_point_id IS NOT NULL BEGIN
 SELECT RAISE(ABORT,'semantic fictional interval is inverted') WHERE
 (SELECT ordinal FROM fictional_time_points WHERE id=NEW.fictional_start_point_id) >
 (SELECT ordinal FROM fictional_time_points WHERE id=NEW.fictional_end_point_id);
END;
CREATE TRIGGER semantic_fictional_point_order_guard BEFORE UPDATE OF ordinal ON fictional_time_points BEGIN
 SELECT RAISE(ABORT,'point reorder would invert a semantic fictional interval') WHERE EXISTS (
 SELECT 1 FROM semantic_fictional_time_bindings b
 JOIN fictional_time_points s ON s.id=b.fictional_start_point_id
 JOIN fictional_time_points e ON e.id=b.fictional_end_point_id
 WHERE b.fictional_start_point_id<>b.fictional_end_point_id AND
 ((b.fictional_start_point_id=OLD.id AND NEW.ordinal>e.ordinal) OR
  (b.fictional_end_point_id=OLD.id AND NEW.ordinal<s.ordinal)));
END;
)sql";
}
