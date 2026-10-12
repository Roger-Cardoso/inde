cmake_minimum_required(VERSION 3.22)

project(inde VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.8)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-i18n>=74)
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES PkgConfig::SQLITE3)
check_cxx_source_compiles("#define SQLITE_ENABLE_SESSION 1\n#include <sqlite3.h>\nint main(){ sqlite3_session* s=nullptr; return sqlite3session_create(nullptr, \"main\", &s); }" INDE_SQLITE_SESSION_AVAILABLE)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT INDE_SQLITE_SESSION_AVAILABLE)
  message(FATAL_ERROR "INDE requires SQLite with ENABLE_SESSION and ENABLE_PREUPDATE_HOOK for Project operation safety")
endif()

add_library(inde_core
    src/project/manifest.cpp
    src/project/project.cpp
    src/project/catalog.cpp
    src/project/structural_node.cpp
    src/project/narrative.cpp
    src/project/editor_interaction.cpp
    src/project/planning.cpp
    src/project/writing.cpp
    src/project/editor_format.cpp
    src/project/document_state.cpp
    src/project/semantic_registry.cpp
    src/project/semantic_validation.cpp
    src/project/linguistic.cpp
    src/project/cartography.cpp
    src/project/structure_model.cpp
    src/persistence/dictionary_store.cpp
    src/persistence/enchant_spelling_provider.cpp
    src/persistence/project_repository.cpp
    src/persistence/json.cpp
    src/persistence/catalog_repository.cpp
    src/persistence/structural_repository.cpp
    src/persistence/recent_projects_store.cpp
    src/persistence/calendar_navigation_store.cpp
    src/persistence/sqlite_database.cpp
    src/persistence/project_operation_store.cpp
    src/persistence/schema_migrator.cpp
    src/persistence/project_database_importer.cpp
    src/persistence/project_database_repository.cpp
    src/persistence/sqlite_catalog_repository.cpp
    src/persistence/sqlite_structural_repository.cpp
    src/persistence/sqlite_structure_model_repository.cpp
    src/persistence/sqlite_narrative_repository.cpp
    src/persistence/sqlite_narrative_entities.cpp
    src/persistence/sqlite_narrative_relations.cpp
    src/persistence/sqlite_narrative_references.cpp
    src/persistence/sqlite_narrative_commands.cpp
    src/persistence/narrative_repository_support.cpp
    src/persistence/narrative_query_support.cpp
    src/persistence/narrative_row_codec.cpp
    src/persistence/sqlite_planning_repository.cpp
    src/persistence/sqlite_writing_repository.cpp
    src/persistence/editor_interaction_store.cpp
    src/persistence/editor_relation_store.cpp
    src/persistence/recovery_codec.cpp
    src/persistence/composition_codec.cpp
    src/persistence/composition_store.cpp
    src/experimental/text_blocks.cpp
    src/experimental/text_blocks_operations.cpp
    src/experimental/text_blocks_snapshot.cpp
    src/experimental/block_layers.cpp
    src/experimental/block_materialization.cpp
    src/persistence/recovery_export.cpp
    src/persistence/semantic_codec.cpp
    src/persistence/inline_command_codec.cpp
    src/persistence/semantic_store.cpp
    src/persistence/semantic_relation_query.cpp
    src/persistence/document_recovery_store.cpp
    src/persistence/sqlite_linguistic_catalog.cpp
    src/persistence/sqlite_cartography_repository.cpp
    src/application/project_service.cpp
    src/application/composition_service.cpp
    src/application/worldbuilding_service.cpp
    src/project/worldbuilding.cpp
    src/project/calendar_view.cpp
    src/project/calendar_engines.cpp
    src/application/project_session.cpp
    src/application/catalog_service.cpp
    src/application/structural_service.cpp
    src/application/structure_model_service.cpp
    src/application/narrative_service.cpp
    src/application/narrative_builder.cpp
    src/application/narrative_builder_alternatives.cpp
    src/application/narrative_realizations.cpp
    src/application/narrative_continuity.cpp
    src/application/planning_service.cpp
    src/application/writing_service.cpp
    src/application/editor_entity_creation.cpp
    src/application/editor_relation_creation.cpp
    src/application/editor_interaction.cpp
    src/application/semantic_distribution.cpp
    src/application/linguistic_service.cpp
    src/application/cartography_service.cpp
    src/application/proofreading_service.cpp
    src/application/text_analysis_coordinator.cpp
    src/application/text_search.cpp
    src/application/document_edit_session.cpp
    src/application/document_export.cpp
    src/application/editorial_review.cpp
    src/application/document_save_coordinator.cpp
    src/application/document_recovery_coordinator.cpp
    src/application/document_recovery_service.cpp
    src/application/text_edit_delta.cpp
    src/application/text_clipboard.cpp
    src/application/document_text_transaction.cpp
    src/application/inline_commands.cpp
    src/application/semantic_anchor_engine.cpp
    src/application/text_semantic_service.cpp
    src/application/timeline_projection.cpp
    src/application/temporal_view.cpp
)
target_include_directories(inde_core PUBLIC include)
target_link_libraries(inde_core PUBLIC PkgConfig::UUID Threads::Threads PRIVATE
    PkgConfig::SQLITE3 PkgConfig::GLIB PkgConfig::ICU ${CMAKE_DL_LIBS})
target_compile_options(inde_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(inde_production
    src/application/work_production.cpp
    src/application/production_packages.cpp
    src/application/production_pdf.cpp)
target_include_directories(inde_production PUBLIC include)
target_link_libraries(inde_production PUBLIC inde_core PRIVATE PkgConfig::GLIB PkgConfig::PANGOCAIRO)
target_compile_options(inde_production PRIVATE -Wall -Wextra -Wpedantic)

add_executable(inde
    src/main.cpp
    src/ui/application_theme.cpp
    src/ui/theme_compatibility.cpp
    src/ui/calendar_picker.cpp
    src/ui/temporal_reference_editor.cpp
    src/ui/temporal_context.cpp
    src/ui/ui_preferences.cpp
    src/ui/keyboard_commands.cpp
    src/ui/main_window_keyboard.cpp
    src/ui/writing_workspace_keyboard.cpp
    src/ui/cartography_workspace_keyboard.cpp
    src/ui/composition_page_keyboard.cpp
    src/ui/workspace_keyboard.cpp
    src/ui/saved_workspaces.cpp
    src/ui/workspace_continuity.cpp
    src/ui/main_window_context.cpp
    src/ui/main_window_operations.cpp
    src/ui/project_operations.cpp
    src/ui/writing_workspace_export.cpp
    src/ui/writing_workspace_review.cpp
    src/ui/rendering_policy.cpp
    src/ui/context_menu.cpp
    src/ui/dictionary_manager_dialog.cpp
    src/ui/main_window.cpp
    src/ui/review_workspace.cpp
    src/ui/review_workspace_detail.cpp
    src/ui/review_workspace_comparison.cpp
    src/ui/narrative_structure_workspace.cpp
    src/ui/narrative_builder_page.cpp
    src/ui/narrative_library_interaction.cpp
    src/ui/work_setup.cpp
    src/ui/narrative_builder_comparison.cpp
    src/ui/narrative_realization_ui.cpp
    src/ui/writing_workspace_narrative.cpp
    src/ui/narrative_structure_workspace_ui.cpp
    src/ui/narrative_structure_workspace_library.cpp
    src/ui/narrative_structure_workspace_detail.cpp
    src/ui/narrative_structure_workspace_roles.cpp
    src/ui/overlay_dialog.cpp
    src/ui/editorial_workspace.cpp
    src/ui/editorial_structure_workspace.cpp
    src/ui/production_page.cpp
    src/ui/composition_page.cpp
    src/ui/graphs_workspace.cpp
    src/ui/cartography_workspace.cpp
    src/ui/cartography_workspace_ui.cpp
    src/ui/cartography_workspace_generation.cpp
    src/ui/cartography_workspace_view.cpp
    src/ui/cartography_workspace_locals.cpp
    src/ui/cartography_workspace_terrain.cpp
    src/ui/incremental_selector.cpp
    src/ui/planning_workspace.cpp
    src/ui/planning_workspace_worldbuilding.cpp
    src/ui/temporal_library_page.cpp
    src/ui/planning_workspace_ui_explorer.cpp
    src/ui/planning_workspace_ui_relations.cpp
    src/ui/planning_workspace_ui_detail.cpp
    src/ui/planning_workspace_ui_time.cpp
    src/ui/planning_workspace_ui_actions.cpp
    src/ui/planning_workspace_explorer.cpp
    src/ui/planning_workspace_explorer_filters.cpp
    src/ui/planning_workspace_entity_types.cpp
    src/ui/planning_workspace_entity_commands.cpp
    src/ui/planning_workspace_entity_detail.cpp
    src/ui/planning_workspace_time_projection.cpp
    src/ui/planning_workspace_time.cpp
    src/ui/planning_workspace_temporal_selectors.cpp
    src/ui/planning_workspace_event_commands.cpp
    src/ui/planning_workspace_presence_commands.cpp
    src/ui/planning_workspace_relations.cpp
    src/ui/planning_workspace_relation_explorer.cpp
    src/ui/planning_workspace_relation_types.cpp
    src/ui/planning_workspace_relation_commands.cpp
    src/ui/planning_workspace_explorer_context.cpp
    src/ui/planning_workspace_context.cpp
    src/ui/proofreading_controller.cpp
    src/ui/structural_editor.cpp
    src/ui/structural_editor_navigation.cpp
    src/ui/structural_editor_model.cpp
    src/ui/structural_editor_inspector.cpp
    src/ui/structural_editor_catalog.cpp
    src/ui/timeline_view.cpp
    src/ui/writing_workspace.cpp
    src/ui/writing_workspace_controls.cpp
    src/ui/editor_color_picker.cpp
    src/ui/writing_workspace_inline.cpp
    src/ui/writing_workspace_interaction.cpp
    src/ui/writing_workspace_text_relation.cpp
    src/ui/writing_workspace_distribution.cpp
    src/ui/writing_workspace_project_inline.cpp
    src/ui/writing_workspace_library.cpp
    src/ui/writing_workspace_document.cpp
    src/ui/writing_workspace_tools.cpp
        src/ui/writing_workspace_search.cpp
    src/ui/writing_workspace_save.cpp
    src/ui/writing_workspace_recovery.cpp
    src/ui/writing_workspace_recovery_ui.cpp
    src/ui/writing_workspace_retention.cpp
        src/ui/writing_workspace_clipboard.cpp
    src/ui/writing_workspace_references.cpp
    src/ui/writing_workspace_linguistic.cpp
    src/ui/writing_workspace_editor.cpp
    src/ui/writing_workspace_ime.cpp
    src/ui/writing_workspace_semantics.cpp
    src/ui/writing_workspace_semantic_inspector.cpp
    src/ui/writing_workspace_semantic_detail.cpp
  src/ui/writing_workspace_semantic_fields.cpp
  src/ui/writing_workspace_semantic_time.cpp
    src/ui/writing_workspace_semantic_uses.cpp
    src/ui/writing_workspace_semantic_relations.cpp
    src/ui/writing_workspace_semantic_relation_picker.cpp
    src/ui/workspace_shell.cpp
)
target_include_directories(inde PRIVATE include)
target_link_libraries(inde PRIVATE inde_production inde_core PkgConfig::GTKMM Threads::Threads)
target_compile_options(inde PRIVATE -Wall -Wextra -Wpedantic)

install(
    TARGETS inde
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES packaging/io.github.inde.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(
    FILES assets/inde.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME inde.png
)

install(
    FILES packaging/io.github.inde.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages
)

install(
    FILES packaging/io.github.inde.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(
    FILES
        LICENSE
        THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/inde
)

include(CTest)
option(INDE_BUILD_TEXT_SPIKE "Build isolated experimental text-block laboratory (not installed)" OFF)
if(INDE_BUILD_TEXT_SPIKE)
    add_library(inde_text_spike
        src/experimental/text_buffer.cpp)
    target_include_directories(inde_text_spike PUBLIC include)
    target_link_libraries(inde_text_spike PUBLIC inde_core PRIVATE PkgConfig::GLIB)
    target_compile_options(inde_text_spike PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(inde_text_storage_spike tools/text_storage_spike.cpp)
    target_link_libraries(inde_text_storage_spike PRIVATE inde_text_spike)
    target_compile_options(inde_text_storage_spike PRIVATE -Wall -Wextra -Wpedantic)
    add_library(inde_text_blocks_lab_ui
        src/experimental/lab_source.cpp
        src/experimental/blocks_lab_window.cpp
        src/experimental/blocks_lab_commands.cpp
        src/experimental/blocks_lab_keyboard.cpp
        src/experimental/blocks_lab_editors.cpp
        src/experimental/blocks_lab_render.cpp
        src/ui/application_theme.cpp
        src/ui/theme_compatibility.cpp
        src/ui/ui_preferences.cpp
        src/ui/keyboard_commands.cpp)
    target_link_libraries(inde_text_blocks_lab_ui PUBLIC inde_text_spike PkgConfig::GTKMM)
    target_compile_options(inde_text_blocks_lab_ui PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(inde_text_blocks_lab tools/blocks_lab_main.cpp src/ui/rendering_policy.cpp)
    target_link_libraries(inde_text_blocks_lab PRIVATE inde_text_blocks_lab_ui)
    add_executable(inde_text_blocks_empty_fixture tools/text_blocks_empty_fixture.cpp)
    target_link_libraries(inde_text_blocks_empty_fixture PRIVATE inde_core)
    if(BUILD_TESTING)
        add_executable(inde_text_blocks_spike_tests tests/text_blocks_spike_tests.cpp)
        target_link_libraries(inde_text_blocks_spike_tests PRIVATE inde_text_spike)
        target_compile_options(inde_text_blocks_spike_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
        add_test(NAME inde_text_blocks_spike_tests COMMAND inde_text_blocks_spike_tests)
        set_tests_properties(inde_text_blocks_spike_tests PROPERTIES TIMEOUT 60)
        add_executable(inde_text_workshop_tests tests/text_workshop_tests.cpp)
        target_link_libraries(inde_text_workshop_tests PRIVATE inde_text_blocks_lab_ui)
        target_compile_options(inde_text_workshop_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
        add_test(NAME inde_text_workshop_tests COMMAND inde_text_workshop_tests)
        set_tests_properties(inde_text_workshop_tests PROPERTIES TIMEOUT 60)
        add_executable(inde_ui_text_workshop_smoke tests/ui_text_workshop_smoke.cpp src/ui/rendering_policy.cpp)
        target_link_libraries(inde_ui_text_workshop_smoke PRIVATE inde_text_blocks_lab_ui)
        target_compile_options(inde_ui_text_workshop_smoke PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    endif()
endif()
if(BUILD_TESTING)
    add_executable(inde_keyboard_policy_tests tests/keyboard_policy_tests.cpp src/ui/keyboard_commands.cpp src/ui/ui_preferences.cpp)
    target_include_directories(inde_keyboard_policy_tests PRIVATE include)
    target_link_libraries(inde_keyboard_policy_tests PRIVATE PkgConfig::GTKMM)
    target_compile_options(inde_keyboard_policy_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_keyboard_policy_tests COMMAND inde_keyboard_policy_tests)
    add_executable(inde_author_vocabulary_tests tests/author_vocabulary_tests.cpp)
    target_link_libraries(inde_author_vocabulary_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_author_vocabulary_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_author_vocabulary_tests COMMAND inde_author_vocabulary_tests)
    add_executable(inde_narrative_builder_tests tests/narrative_builder_tests.cpp)
    add_executable(inde_work_setup_tests tests/work_setup_tests.cpp)
    target_link_libraries(inde_work_setup_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_work_setup_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_work_setup_tests COMMAND inde_work_setup_tests)
    target_link_libraries(inde_narrative_builder_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_narrative_builder_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_narrative_builder_tests COMMAND inde_narrative_builder_tests)
    add_executable(inde_narrative_alternatives_tests tests/narrative_alternatives_tests.cpp)
    target_link_libraries(inde_narrative_alternatives_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_narrative_alternatives_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_narrative_alternatives_tests COMMAND inde_narrative_alternatives_tests)
    add_executable(inde_narrative_realization_tests tests/narrative_realization_tests.cpp)
    target_link_libraries(inde_narrative_realization_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_narrative_realization_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_narrative_realization_tests COMMAND inde_narrative_realization_tests)
    add_executable(inde_calendar_view_tests tests/calendar_view_tests.cpp)
    target_link_libraries(inde_calendar_view_tests PRIVATE inde_core)
    target_compile_options(inde_calendar_view_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_calendar_view_tests COMMAND inde_calendar_view_tests)
    add_executable(inde_project_operation_tests tests/project_operation_tests.cpp)
    add_executable(inde_worldbuilding_tests tests/worldbuilding_tests.cpp)
    target_link_libraries(inde_worldbuilding_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_worldbuilding_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_worldbuilding_tests COMMAND inde_worldbuilding_tests)
    add_executable(inde_temporal_system_tests tests/temporal_system_tests.cpp)
    target_link_libraries(inde_temporal_system_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_temporal_system_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_temporal_system_tests COMMAND inde_temporal_system_tests)
    add_executable(inde_temporal_templates_tests tests/temporal_templates_tests.cpp)
    target_link_libraries(inde_temporal_templates_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_temporal_templates_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_temporal_templates_tests COMMAND inde_temporal_templates_tests)
    add_executable(inde_temporal_context_tests tests/temporal_context_tests.cpp)
    add_executable(inde_temporal_workflow_tests tests/temporal_workflow_tests.cpp)
    target_link_libraries(inde_temporal_workflow_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_temporal_workflow_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_temporal_workflow_tests COMMAND inde_temporal_workflow_tests)
    target_link_libraries(inde_temporal_context_tests PRIVATE inde_core PkgConfig::GLIB)
    if(TARGET inde_text_spike)
      target_link_libraries(inde_temporal_context_tests PRIVATE inde_text_spike)
      target_compile_definitions(inde_temporal_context_tests PRIVATE INDE_TEMPORAL_CONTEXT_LAB)
    endif()
    target_compile_options(inde_temporal_context_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_temporal_context_tests COMMAND inde_temporal_context_tests)
    add_executable(inde_composition_tests tests/composition_tests.cpp)
    target_link_libraries(inde_composition_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_composition_tests PRIVATE -UNDEBUG)
    add_test(NAME inde_composition_tests COMMAND inde_composition_tests)
    target_link_libraries(inde_project_operation_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_project_operation_tests PRIVATE -UNDEBUG)
    add_test(NAME inde_project_operation_tests COMMAND inde_project_operation_tests)
    add_executable(inde_removal_assistant_tests tests/removal_assistant_tests.cpp)
    target_link_libraries(inde_removal_assistant_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_removal_assistant_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_removal_assistant_tests COMMAND inde_removal_assistant_tests)
    add_executable(inde_work_production_tests tests/work_production_tests.cpp)
    target_link_libraries(inde_work_production_tests PRIVATE inde_production)
    target_compile_options(inde_work_production_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_work_production_tests COMMAND inde_work_production_tests)
    set_tests_properties(inde_work_production_tests PROPERTIES TIMEOUT 60)
    add_executable(inde_prepare_editorial_review_copy tools/prepare_editorial_review_copy.cpp)
    target_link_libraries(inde_prepare_editorial_review_copy PRIVATE inde_core)
    target_compile_options(inde_prepare_editorial_review_copy PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(inde_editorial_review_tests tests/editorial_review_tests.cpp)
    target_link_libraries(inde_editorial_review_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_editorial_review_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_editorial_review_tests COMMAND inde_editorial_review_tests)
    set_tests_properties(inde_editorial_review_tests PROPERTIES TIMEOUT 60)
    add_executable(inde_continuity_production_tests tests/continuity_production_tests.cpp
        src/ui/saved_workspaces.cpp src/ui/ui_preferences.cpp)
    target_link_libraries(inde_continuity_production_tests PRIVATE inde_core PkgConfig::GLIB)
    target_compile_options(inde_continuity_production_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_continuity_production_tests COMMAND inde_continuity_production_tests)
    add_executable(inde_semantic_increment_tests tests/semantic_increment_tests.cpp)
    add_executable(inde_editor_toolbar_tests tests/editor_toolbar_tests.cpp)
    target_link_libraries(inde_editor_toolbar_tests PRIVATE inde_core)
    target_compile_options(inde_editor_toolbar_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_editor_toolbar_tests COMMAND inde_editor_toolbar_tests)
    set_tests_properties(inde_editor_toolbar_tests PROPERTIES TIMEOUT 60)
    target_link_libraries(inde_semantic_increment_tests PRIVATE inde_core)
    target_compile_options(inde_semantic_increment_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_semantic_increment_tests COMMAND inde_semantic_increment_tests)
    set_tests_properties(inde_semantic_increment_tests PROPERTIES TIMEOUT 60)
    add_executable(inde_text_semantic_tests tests/text_semantic_tests.cpp)
    target_link_libraries(inde_text_semantic_tests PRIVATE inde_core)
    target_compile_options(inde_text_semantic_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_text_semantic_tests COMMAND inde_text_semantic_tests)
    set_tests_properties(inde_text_semantic_tests PROPERTIES TIMEOUT 45)
    add_executable(inde_demo_project_generator
        tools/demo_project_generator.cpp
        tools/demo_project/demo_builder.cpp
        tools/demo_project/demo_catalog.cpp
        tools/demo_project/demo_entities.cpp
        tools/demo_project/demo_planning.cpp
        tools/demo_project/demo_documents.cpp
        tools/demo_project/demo_manuscript.cpp
        tools/demo_project/demo_complementary_texts.cpp)
    target_link_libraries(inde_demo_project_generator PRIVATE inde_core)
    target_compile_options(inde_demo_project_generator PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_colossal_fixture_generator
        tools/colossal_fixture_generator.cpp)
    target_link_libraries(inde_colossal_fixture_generator PRIVATE inde_core)
    target_compile_options(inde_colossal_fixture_generator PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_core_tests
        tests/core_tests.cpp
        tests/core/project_lifecycle_tests.cpp
        tests/core/proofreading_tests.cpp
        tests/core/structural_tree_tests.cpp
        tests/core/sqlite_schema_tests.cpp
        tests/core/sqlite_import_tests.cpp
        tests/core/structural_model_tests.cpp
        tests/core/narrative_tests.cpp
        tests/core/writing_tests.cpp
        tests/core/planning_tests.cpp
        tests/core/timeline_tests.cpp
        tests/core/sqlite_fixture.cpp)
    target_link_libraries(inde_core_tests PRIVATE inde_core)
    target_compile_options(inde_core_tests PRIVATE -UNDEBUG)
    add_test(NAME inde_core_tests COMMAND inde_core_tests)
    add_executable(inde_text_analysis_tests tests/text_analysis_coordinator_tests.cpp)
    add_executable(inde_semantic_context_tests tests/semantic_context_tests.cpp)
    add_executable(inde_editor_interaction_tests tests/editor_interaction_tests.cpp)
    target_link_libraries(inde_editor_interaction_tests PRIVATE inde_core)
    target_compile_options(inde_editor_interaction_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_editor_interaction_tests COMMAND inde_editor_interaction_tests)
    target_link_libraries(inde_semantic_context_tests PRIVATE inde_core)
    target_compile_options(inde_semantic_context_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_semantic_context_tests COMMAND inde_semantic_context_tests)
    target_link_libraries(inde_text_analysis_tests PRIVATE inde_core)
    target_compile_options(inde_text_analysis_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_text_analysis_tests COMMAND inde_text_analysis_tests)
    add_executable(inde_document_edit_tests tests/document_edit_session_tests.cpp)
    target_link_libraries(inde_document_edit_tests PRIVATE inde_core)
    target_compile_options(inde_document_edit_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_document_edit_tests COMMAND inde_document_edit_tests)
    add_executable(inde_editor_scale_tests tests/editor_scale_tests.cpp)
    target_link_libraries(inde_editor_scale_tests PRIVATE inde_core)
    target_compile_options(inde_editor_scale_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_editor_scale_tests COMMAND inde_editor_scale_tests)
    set_tests_properties(inde_editor_scale_tests PROPERTIES TIMEOUT 30)
    add_executable(inde_document_recovery_tests tests/document_recovery_tests.cpp)
    target_link_libraries(inde_document_recovery_tests PRIVATE inde_core)
    target_compile_options(inde_document_recovery_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_document_recovery_tests COMMAND inde_document_recovery_tests)
    add_executable(inde_recovery_retention_tests tests/recovery_retention_tests.cpp)
    target_link_libraries(inde_recovery_retention_tests PRIVATE inde_core)
    target_compile_options(inde_recovery_retention_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME inde_recovery_retention_tests COMMAND inde_recovery_retention_tests)
    set_tests_properties(inde_document_recovery_tests PROPERTIES TIMEOUT 90)
    add_executable(inde_rendering_policy_tests
        tests/rendering_policy_tests.cpp src/ui/rendering_policy.cpp)
    target_link_libraries(inde_rendering_policy_tests PRIVATE PkgConfig::GLIB PkgConfig::GTKMM)
    target_compile_options(inde_rendering_policy_tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME inde_rendering_policy_tests COMMAND inde_rendering_policy_tests)
    add_executable(inde_theme_compatibility_tests
        tests/theme_compatibility_tests.cpp src/ui/theme_compatibility.cpp)
    target_link_libraries(inde_theme_compatibility_tests PRIVATE PkgConfig::GLIB)
    target_compile_options(inde_theme_compatibility_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME inde_theme_compatibility_tests COMMAND inde_theme_compatibility_tests)
    add_executable(inde_ui_theme_compatibility_smoke
        tests/ui_theme_compatibility_smoke.cpp src/ui/theme_compatibility.cpp
        src/ui/application_theme.cpp src/ui/ui_preferences.cpp src/ui/rendering_policy.cpp)
    target_include_directories(inde_ui_theme_compatibility_smoke PRIVATE include)
    target_link_libraries(inde_ui_theme_compatibility_smoke PRIVATE PkgConfig::GTKMM)
    target_compile_options(inde_ui_theme_compatibility_smoke PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(inde_cartography_tests tests/cartography_tests.cpp)
    target_link_libraries(inde_cartography_tests PRIVATE inde_core)
    target_compile_options(inde_cartography_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_cartography_tests COMMAND inde_cartography_tests)

    add_executable(inde_linguistic_integration_tests
        tests/linguistic_integration_tests.cpp)
    target_link_libraries(inde_linguistic_integration_tests PRIVATE inde_core)
    target_compile_options(inde_linguistic_integration_tests PRIVATE
        -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_linguistic_integration_tests
        COMMAND inde_linguistic_integration_tests)

    add_executable(inde_narrative_benchmark tests/narrative_benchmark.cpp)
    target_link_libraries(inde_narrative_benchmark PRIVATE inde_core)
    target_compile_options(inde_narrative_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_timeline_benchmark tests/timeline_benchmark.cpp)
    target_link_libraries(inde_timeline_benchmark PRIVATE inde_core)
    target_compile_options(inde_timeline_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_writing_benchmark tests/writing_benchmark.cpp)
    target_link_libraries(inde_writing_benchmark PRIVATE inde_core)
    target_compile_options(inde_writing_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_workspace_benchmark tests/workspace_benchmark.cpp)
    target_link_libraries(inde_workspace_benchmark PRIVATE inde_core)
    target_compile_options(inde_workspace_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_proofreading_benchmark
        tests/proofreading_benchmark.cpp)
    target_link_libraries(inde_proofreading_benchmark PRIVATE inde_core)
    target_compile_options(inde_proofreading_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
endif()
