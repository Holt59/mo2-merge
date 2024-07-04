cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/common/mo2.cmake)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/usvfs/install/lib/cmake)

function (mo2_add_subdirectories)
	cmake_parse_arguments(MO2 "" "FOLDER" "GLOB" ${ARGN})

    if (NOT DEFINED MO2_FOLDER)
        message(FATAL_ERROR  "missing FOLDER in add_subdirectories")
    endif()
    if (NOT DEFINED MO2_GLOB)
        message(FATAL_ERROR  "missing GLOB in add_subdirectories")
    endif()

    file(GLOB directories RELATIVE ${CMAKE_CURRENT_LIST_DIR} LIST_DIRECTORIES TRUE ${MO2_GLOB})

    set(CMAKE_FOLDER ${MO2_FOLDER})
    foreach(directory ${directories})
        add_subdirectory(${directory})
    endforeach()
    unset(CMAKE_FOLDER)

endfunction()

project(modorganizer2)

set(USE_FOLDERS TRUE)

add_subdirectory(uibase)

# set(CMAKE_FOLDER python)
# add_subdirectory(python)
# unset(CMAKE_FOLDER)

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
