#include "core/core_test_cases.hpp"

#include <iostream>

int main() {
  inde::tests::project_round_trip();
  inde::tests::rejects_future_format();
  inde::tests::strict_json_codec();
  inde::tests::dictionaries_and_proofreading_are_global_and_persistent();
  inde::tests::recents_are_deduplicated();
  inde::tests::recents_failure_does_not_block_project_lifecycle();
  inde::tests::failed_open_preserves_active_session();
  inde::tests::specialized_services_share_a_guarded_session();
  inde::tests::catalog_round_trip();
  inde::tests::structural_tree_round_trip_and_integrity();
  inde::tests::structural_path_labels_disambiguate_repeated_titles();
  inde::tests::legacy_structural_positions_are_compatible();
  inde::tests::structural_templates_and_duplication();
  inde::tests::large_structural_tree();
  inde::tests::sqlite_schema_and_transactions();
  inde::tests::sqlite_database_closes_before_copy();
  inde::tests::sqlite_v11_migrates_structural_identity_and_per_type_designators();
  inde::tests::sqlite_foreign_keys_and_editorial_parent_integrity();
  inde::tests::sqlite_rejects_future_schema();
  inde::tests::sqlite_rolls_back_interrupted_migration();
  inde::tests::sqlite_imports_json_project_safely();
  inde::tests::sqlite_import_rejects_invalid_json_without_artifacts();
  inde::tests::sqlite_imports_empty_json_project();
  inde::tests::sqlite_import_rejects_orphan_work_without_artifacts();
  inde::tests::project_service_uses_sqlite_as_the_only_operational_store();
  inde::tests::structural_element_types_and_designators_are_managed();
  inde::tests::structure_models_instances_and_narrative_flow_are_independent();
  inde::tests::legacy_project_is_upgraded_once_and_v2_requires_its_database();
  inde::tests::sqlite_structural_batches_are_atomic();
  inde::tests::narrative_foundation_enforces_identity_and_relations();
  inde::tests::narrative_foundation_survives_save_as_independently();
  inde::tests::writing_documents_persist_without_collapsing_editorial_identity();
  inde::tests::textual_references_preserve_identity_and_protect_sources();
  inde::tests::planning_places_events_in_explicit_fictional_time();
  inde::tests::temporal_queries_are_limited_inside_the_selected_axis();
  inde::tests::narrative_editorial_links_preserve_layer_boundaries();
  inde::tests::narrative_queries_remain_bounded_at_scale();
  inde::tests::planning_context_combines_filters_and_explains_results();
  inde::tests::timeline_projection_is_deterministic_and_selectable();
  inde::tests::temporal_view_context_has_explicit_window_scope_and_limits();
  std::cout << "Todos os testes do núcleo passaram.\n";
}
