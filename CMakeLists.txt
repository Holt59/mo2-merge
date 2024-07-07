cmake_minimum_required(VERSION 3.23)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/usvfs/install/lib/cmake)

project(modorganizer2 CXX)

# include after project() otherwise find_package(Qt6) will fail
include(${CMAKE_CURRENT_LIST_DIR}/cmake/common/mo2.cmake)

# do this here for most components to speed-up configure time
find_package(Qt6 6.7.0 COMPONENTS
    Concurrent Core Network Qml Quick QuickWidgets WebEngineWidgets WebSockets Widgets)

# add the various directory

add_subdirectory(uibase)

mo2_add_subdirectories(FOLDER python GLOB python)

mo2_add_subdirectories(FOLDER libs GLOB libs/*)

mo2_add_subdirectories(FOLDER plugins/games GLOB plugins/game_* plugins/basic_games)
mo2_add_subdirectories(FOLDER plugins/installers GLOB plugins/installer_*)
mo2_add_subdirectories(FOLDER plugins/tools GLOB
    plugins/bsa_extractor plugins/bsapacker plugins/fnistool plugins/tool_*)
mo2_add_subdirectories(FOLDER plugins/diagnose GLOB
    plugins/check_fnis plugins/diagnose_basic plugins/form43_checker
    plugins/script_extender_plugin_checker)
mo2_add_subdirectories(FOLDER plugins/preview GLOB plugins/preview_*)

mo2_add_subdirectories(FOLDER tools GLOB tools/*)

add_subdirectory(modorganizer)

set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT organizer)
