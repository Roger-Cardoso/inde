#pragma once
namespace inde::persistence::schema_sql {
inline constexpr auto schema_version_28 = R"sql(
CREATE UNIQUE INDEX compositions_identity_owner ON compositions(id,project_id);
CREATE TABLE narrative_text_targets (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, unit_id TEXT NOT NULL,
 document_id TEXT, anchor_id TEXT, composition_id TEXT, occurrence_id TEXT,
 observed_sha TEXT NOT NULL CHECK(length(observed_sha)=64),
 note TEXT NOT NULL DEFAULT '' CHECK(length(note)<=2048),
 review_state TEXT NOT NULL DEFAULT 'unchecked' CHECK(review_state IN('unchecked','confirmed','diverged')),
 provenance TEXT NOT NULL DEFAULT '' CHECK(length(provenance)<=8192),
 CHECK((document_id IS NOT NULL AND anchor_id IS NOT NULL AND composition_id IS NULL AND occurrence_id IS NULL)
 OR (document_id IS NULL AND anchor_id IS NULL AND composition_id IS NOT NULL AND occurrence_id IS NOT NULL)),
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(composition_id,project_id) REFERENCES compositions(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
);
-- Missing anchor/occurrence identities survive for explicit reconciliation.
CREATE UNIQUE INDEX narrative_text_target_unique ON narrative_text_targets(unit_id,coalesce(document_id,''),coalesce(anchor_id,''),coalesce(composition_id,''),coalesce(occurrence_id,''));
CREATE INDEX narrative_text_target_document ON narrative_text_targets(project_id,document_id);
CREATE INDEX narrative_text_target_composition ON narrative_text_targets(project_id,composition_id);
CREATE TABLE narrative_text_decisions (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, unit_id TEXT NOT NULL,
 realization_id TEXT NOT NULL, decision TEXT NOT NULL,
 evidence TEXT NOT NULL CHECK(length(evidence)<=8192), created_at TEXT NOT NULL,
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE TABLE narrative_continuity_exceptions (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, structure_id TEXT NOT NULL,
 notice_key TEXT NOT NULL, evidence_sha TEXT NOT NULL CHECK(length(evidence_sha)=64),
 note TEXT NOT NULL CHECK(length(note)>0 AND length(note)<=2048),
 created_at TEXT NOT NULL, UNIQUE(structure_id,notice_key),
 FOREIGN KEY(structure_id,project_id) REFERENCES narrative_structures(id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE TABLE narrative_unit_presence_intents (
 id TEXT PRIMARY KEY, project_id TEXT NOT NULL, unit_id TEXT NOT NULL, entity_id TEXT NOT NULL,
 UNIQUE(unit_id,entity_id),
 FOREIGN KEY(unit_id,project_id) REFERENCES narrative_units(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(entity_id,project_id) REFERENCES entities(id,project_id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
);
)sql";
}
