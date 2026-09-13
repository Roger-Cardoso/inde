cmake_minimum_required(VERSION 3.22)

project(inde VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.8)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_library(inde_core
    src/project/manifest.cpp
    src/project/project.cpp
    src/project/catalog.cpp
    src/project/structural_node.cpp
    src/project/narrative.cpp
    src/project/planning.cpp
    src/project/writing.cpp
    src/project/cartography.cpp
    src/project/structure_model.cpp
    src/persistence/dictionary_store.cpp
    src/persistence/enchant_spelling_provider.cpp
    src/persistence/project_repository.cpp
    src/persistence/json.cpp
    src/persistence/catalog_repository.cpp
    src/persistence/structural_repository.cpp
    src/persistence/recent_projects_store.cpp
    src/persistence/sqlite_database.cpp
    src/persistence/schema_migrator.cpp
    src/persistence/project_database_importer.cpp
    src/persistence/project_database_repository.cpp
    src/persistence/sqlite_catalog_repository.cpp
    src/persistence/sqlite_structural_repository.cpp
    src/persistence/sqlite_structure_model_repository.cpp
    src/persistence/sqlite_narrative_repository.cpp
    src/persistence/sqlite_planning_repository.cpp
    src/persistence/sqlite_writing_repository.cpp
    src/persistence/sqlite_cartography_repository.cpp
    src/application/project_service.cpp
    src/application/project_session.cpp
    src/application/catalog_service.cpp
    src/application/structural_service.cpp
    src/application/structure_model_service.cpp
    src/application/narrative_service.cpp
    src/application/planning_service.cpp
    src/application/writing_service.cpp
    src/application/cartography_service.cpp
    src/application/proofreading_service.cpp
    src/application/timeline_projection.cpp
    src/application/temporal_view.cpp
)
target_include_directories(inde_core PUBLIC include)
target_link_libraries(inde_core PUBLIC PkgConfig::UUID PRIVATE PkgConfig::SQLITE3 ${CMAKE_DL_LIBS})
target_compile_options(inde_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(inde
    src/main.cpp
    src/ui/application_theme.cpp
    src/ui/context_menu.cpp
    src/ui/dictionary_manager_dialog.cpp
    src/ui/main_window.cpp
    src/ui/narrative_structure_workspace.cpp
    src/ui/overlay_dialog.cpp
    src/ui/editorial_workspace.cpp
    src/ui/editorial_structure_workspace.cpp
    src/ui/graphs_workspace.cpp
    src/ui/cartography_workspace.cpp
    src/ui/incremental_selector.cpp
    src/ui/planning_workspace.cpp
    src/ui/proofreading_controller.cpp
    src/ui/structural_editor.cpp
    src/ui/timeline_view.cpp
    src/ui/writing_workspace.cpp
    src/ui/workspace_shell.cpp
)
target_include_directories(inde PRIVATE include)
target_link_libraries(inde PRIVATE inde_core PkgConfig::GTKMM Threads::Threads)
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
if(BUILD_TESTING)
    add_executable(inde_demo_project_generator tools/demo_project_generator.cpp)
    target_link_libraries(inde_demo_project_generator PRIVATE inde_core)
    target_compile_options(inde_demo_project_generator PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_colossal_fixture_generator
        tools/colossal_fixture_generator.cpp)
    target_link_libraries(inde_colossal_fixture_generator PRIVATE inde_core)
    target_compile_options(inde_colossal_fixture_generator PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(inde_core_tests tests/core_tests.cpp)
    target_link_libraries(inde_core_tests PRIVATE inde_core)
    target_compile_options(inde_core_tests PRIVATE -UNDEBUG)
    add_test(NAME inde_core_tests COMMAND inde_core_tests)
    add_executable(inde_cartography_tests tests/cartography_tests.cpp)
    target_link_libraries(inde_cartography_tests PRIVATE inde_core)
    target_compile_options(inde_cartography_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    add_test(NAME inde_cartography_tests COMMAND inde_cartography_tests)

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
