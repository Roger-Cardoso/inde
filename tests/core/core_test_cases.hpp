#pragma once

namespace inde::tests {

void project_round_trip();
void rejects_future_format();
void strict_json_codec();
void dictionaries_and_proofreading_are_global_and_persistent();
void recents_are_deduplicated();
void recents_failure_does_not_block_project_lifecycle();
void failed_open_preserves_active_session();
void specialized_services_share_a_guarded_session();
void catalog_round_trip();
void structural_tree_round_trip_and_integrity();
void structural_path_labels_disambiguate_repeated_titles();
void legacy_structural_positions_are_compatible();
void structural_templates_and_duplication();
void large_structural_tree();
void sqlite_schema_and_transactions();
void sqlite_database_closes_before_copy();
void sqlite_v11_migrates_structural_identity_and_per_type_designators();
void sqlite_foreign_keys_and_editorial_parent_integrity();
void sqlite_rejects_future_schema();
void sqlite_rolls_back_interrupted_migration();
void sqlite_imports_json_project_safely();
void sqlite_import_rejects_invalid_json_without_artifacts();
void sqlite_imports_empty_json_project();
void sqlite_import_rejects_orphan_work_without_artifacts();
void project_service_uses_sqlite_as_the_only_operational_store();
void structural_element_types_and_designators_are_managed();
void structure_models_instances_and_narrative_flow_are_independent();
void legacy_project_is_upgraded_once_and_v2_requires_its_database();
void sqlite_structural_batches_are_atomic();
void narrative_foundation_enforces_identity_and_relations();
void narrative_foundation_survives_save_as_independently();
void writing_documents_persist_without_collapsing_editorial_identity();
void textual_references_preserve_identity_and_protect_sources();
void planning_places_events_in_explicit_fictional_time();
void temporal_queries_are_limited_inside_the_selected_axis();
void narrative_editorial_links_preserve_layer_boundaries();
void narrative_queries_remain_bounded_at_scale();
void planning_context_combines_filters_and_explains_results();
void timeline_projection_is_deterministic_and_selectable();
void temporal_view_context_has_explicit_window_scope_and_limits();

} // namespace inde::tests
