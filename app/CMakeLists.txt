# afrorecomp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(afrorecomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(AFRORECOMP_SOURCES
    src/main.cpp
    src/language_picker.cpp
    src/frame_pacing.cpp
    src/afro_menu.cpp
)

# Single version source: app/VERSION (bump per release).
file(STRINGS VERSION AFRO_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

# Game icon: generated privately by tools/extract_icon.py (gitignored).
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/afro_icon.ico)
    enable_language(RC)
    list(APPEND AFRORECOMP_SOURCES src/afro_icon.rc)
endif()

if(WIN32)
    add_executable(afrorecomp WIN32 ${AFRORECOMP_SOURCES})
else()
    add_executable(afrorecomp ${AFRORECOMP_SOURCES})
endif()

rexglue_setup_target(afrorecomp GPU_PLUGINS xenos)
target_compile_definitions(afrorecomp PRIVATE AFRO_VERSION="${AFRO_VERSION}")
# region/afro_region.h: written by tools/region_config.py (tools/build_win.bat).
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/region/afro_region.h)
    message(FATAL_ERROR "app/region/afro_region.h is missing: run python tools/region_config.py app")
endif()
target_include_directories(afrorecomp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/region)
