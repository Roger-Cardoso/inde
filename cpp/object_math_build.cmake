# Pinned MIT MicroTeX core; GTK4/Cairo bridge is maintained in this repository.
file(GLOB MATH_CORE CONFIGURE_DEPENDS
  vendor/microtex/src/atom/*.cpp vendor/microtex/src/box/*.cpp
  vendor/microtex/src/core/*.cpp vendor/microtex/src/fonts/*.cpp
  vendor/microtex/src/utils/*.cpp vendor/microtex/src/res/builtin/*.cpp
  vendor/microtex/src/res/parser/*.cpp vendor/microtex/src/res/reg/*.cpp
  vendor/microtex/src/res/font/*.cpp vendor/microtex/src/res/sym/*.cpp)
add_library(inde_math_core STATIC ${MATH_CORE} vendor/microtex/src/latex.cpp
 vendor/microtex/src/render.cpp vendor/tinyxml2/tinyxml2.cpp)
target_include_directories(inde_math_core SYSTEM PUBLIC vendor/microtex/src)
target_include_directories(inde_math_core PRIVATE vendor/tinyxml2 ${CMAKE_CURRENT_SOURCE_DIR})
# The imported library is compiled with its upstream warning profile. INDE's
# own wrapper/editor keep the repository's strict diagnostics.
set_property(TARGET inde_math_core PROPERTY COMPILE_OPTIONS "")
target_compile_definitions(inde_math_core PRIVATE BUILD_GTK)
pkg_check_modules(MATH_FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
target_link_libraries(inde_math_core PUBLIC PkgConfig::GTKMM PkgConfig::MATH_FONTCONFIG)
add_library(inde_math_native STATIC object_math_native.cpp object_math_cairo.cpp)
target_link_libraries(inde_math_native PUBLIC inde_math_core)
target_compile_definitions(inde_math_native PRIVATE BUILD_GTK INDE_MATH_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vendor/microtex/res")
target_compile_options(inde_math_native PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
install(DIRECTORY vendor/microtex/res/ DESTINATION share/inde/math)
install(FILES vendor/microtex/LICENSE DESTINATION share/inde/licenses/MicroTeX)
install(FILES vendor/tinyxml2/LICENSE.txt DESTINATION share/inde/licenses/TinyXML2)
add_executable(inde_native_math_tests object_math_native_tests.cpp)
target_link_libraries(inde_native_math_tests PRIVATE inde_math_native)
if(INDE_BUILD_CORE_TESTS)
 add_test(NAME inde_native_math_tests COMMAND inde_native_math_tests)
endif()
