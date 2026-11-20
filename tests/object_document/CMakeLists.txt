cmake_minimum_required(VERSION 3.22)
project(inde_obj01_document_read LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
option(OBJ01_SANITIZE "Instrument adapter and all included owner sources" OFF)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
get_filename_component(OBJ01_ROOT ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(OBJ01_CPP ${OBJ01_ROOT}/cpp)
set(OBJ01_OWNER ${CMAKE_CURRENT_BINARY_DIR}/owner-source)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Reuse the existing verified, build-local transformations. No GTK/SQLite target
# is configured, and no archived or extracted original source is modified.
execute_process(COMMAND ${Python3_EXECUTABLE} ${OBJ01_ROOT}/verificar_base.py --integrado
                COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${Python3_EXECUTABLE} ${OBJ01_CPP}/otimizar_base.py
                ${OBJ01_ROOT}/fontes/inde-integrado-schema37 ${OBJ01_OWNER} OFF
                COMMAND_ERROR_IS_FATAL ANY)
if(OBJ01_SANITIZE)
  add_compile_options(-g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
add_library(inde_document_owner STATIC
  ${OBJ01_OWNER}/src/project/writing.cpp
  ${OBJ01_OWNER}/src/project/editor_format.cpp
  ${OBJ01_OWNER}/src/project/manifest.cpp
  ${OBJ01_OWNER}/src/project/document_state.cpp
  ${OBJ01_OWNER}/src/project/document_paragraphs.cpp
  ${OBJ01_OWNER}/src/project/document_alternatives.cpp
  ${OBJ01_OWNER}/src/project/document_visibility.cpp
  ${OBJ01_OWNER}/src/project/semantic_validation.cpp
  ${OBJ01_OWNER}/src/project/semantic_registry.cpp
  ${OBJ01_OWNER}/src/application/document_reading_order.cpp
  ${OBJ01_OWNER}/src/application/semantic_anchor_engine.cpp
  ${OBJ01_OWNER}/src/persistence/json.cpp
  ${OBJ01_OWNER}/src/persistence/recovery_codec.cpp
  ${OBJ01_OWNER}/src/persistence/semantic_codec.cpp
  ${OBJ01_OWNER}/src/persistence/inline_command_codec.cpp
  ${OBJ01_OWNER}/src/persistence/paragraph_codec.cpp
  ${OBJ01_OWNER}/src/persistence/alternatives_codec.cpp
  ${OBJ01_OWNER}/src/persistence/visibility_codec.cpp)
target_include_directories(inde_document_owner PUBLIC ${OBJ01_OWNER}/include)
target_link_libraries(inde_document_owner PUBLIC PkgConfig::GLIB PkgConfig::UUID)
target_compile_options(inde_document_owner PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Frozen owner code predates Clang's -Wconversion => -Wsign-conversion.
  # Keep the exception local to those unchanged sources. New adapter/tests
  # retain the complete strict warning profile on both compilers.
  target_compile_options(inde_document_owner PRIVATE -Wno-sign-conversion)
endif()
add_library(inde_object_document_adapter STATIC ${OBJ01_CPP}/object_model.cpp
  ${OBJ01_CPP}/object_commands.cpp ${OBJ01_CPP}/object_document_adapter.cpp)
target_include_directories(inde_object_document_adapter PUBLIC ${OBJ01_CPP})
target_link_libraries(inde_object_document_adapter PUBLIC inde_document_owner)
target_compile_options(inde_object_document_adapter PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
add_executable(inde_object_document_adapter_tests ${OBJ01_CPP}/object_document_adapter_tests.cpp)
target_link_libraries(inde_object_document_adapter_tests PRIVATE inde_object_document_adapter)
target_compile_options(inde_object_document_adapter_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
add_test(NAME inde_object_document_adapter_tests COMMAND inde_object_document_adapter_tests)
set_tests_properties(inde_object_document_adapter_tests PROPERTIES TIMEOUT 120)
