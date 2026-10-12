#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_25=R"sql(
CREATE TABLE temporal_reference_extents (
 reference_id TEXT PRIMARY KEY, project_id TEXT NOT NULL, payload TEXT NOT NULL,
 FOREIGN KEY(reference_id,project_id) REFERENCES temporal_references(id,project_id)
 ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
ALTER TABLE temporal_attachments ADD COLUMN meaning TEXT NOT NULL DEFAULT 'unspecified'
 CHECK(meaning IN ('unspecified','birth','death','start','end','occurrence','mention'));
CREATE TABLE semantic_date_bindings (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL,
 annotation_id TEXT NOT NULL, reference_id TEXT NOT NULL, role TEXT NOT NULL,
 FOREIGN KEY(annotation_id,document_id,project_id) REFERENCES semantic_annotations(id,document_id,project_id)
 ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(reference_id,project_id) REFERENCES temporal_references(id,project_id)
 DEFERRABLE INITIALLY DEFERRED,
 UNIQUE(annotation_id,reference_id,role)
);
CREATE INDEX semantic_date_reference ON semantic_date_bindings(project_id,reference_id,document_id,annotation_id);
)sql";
}
