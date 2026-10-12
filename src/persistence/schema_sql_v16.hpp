#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_16=R"sql(
CREATE TABLE semantic_type_definitions (
 project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
 type_key TEXT NOT NULL, type_version INTEGER NOT NULL CHECK(type_version>0),
 definition_json TEXT NOT NULL, PRIMARY KEY(project_id,type_key,type_version)
);
CREATE TABLE document_semantics (
 document_id TEXT PRIMARY KEY, project_id TEXT NOT NULL, payload TEXT NOT NULL,
 FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE TABLE semantic_annotations (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL,
 anchor_id TEXT NOT NULL, type_key TEXT NOT NULL, type_version INTEGER NOT NULL CHECK(type_version>0),
 status TEXT NOT NULL CHECK(status IN ('active','archived','incomplete')), category TEXT NOT NULL DEFAULT '',
 UNIQUE(id,project_id), UNIQUE(id,document_id,project_id),
 FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(anchor_id,document_id,project_id) REFERENCES document_anchors(id,document_id,project_id) DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX semantic_annotations_scope ON semantic_annotations(project_id,type_key,status,document_id,id);
CREATE INDEX semantic_annotations_category ON semantic_annotations(project_id,category,document_id,id);
CREATE TABLE semantic_entity_bindings (
 project_id TEXT NOT NULL, document_id TEXT NOT NULL, annotation_id TEXT NOT NULL,
 field_key TEXT NOT NULL, ordinal INTEGER NOT NULL, entity_id TEXT NOT NULL,
 PRIMARY KEY(annotation_id,field_key,ordinal),
 FOREIGN KEY(annotation_id,document_id,project_id) REFERENCES semantic_annotations(id,document_id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(entity_id,project_id) REFERENCES entities(id,project_id) DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX semantic_entity_uses ON semantic_entity_bindings(project_id,entity_id,annotation_id);
CREATE TABLE semantic_annotation_relations (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL,
 source_id TEXT NOT NULL, target_id TEXT NOT NULL, type_key TEXT NOT NULL,
 FOREIGN KEY(source_id,document_id,project_id) REFERENCES semantic_annotations(id,document_id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(target_id,project_id) REFERENCES semantic_annotations(id,project_id) DEFERRABLE INITIALLY DEFERRED,
 CHECK(source_id!=target_id)
);
CREATE INDEX semantic_relation_incoming ON semantic_annotation_relations(project_id,target_id,source_id);
)sql";
}
