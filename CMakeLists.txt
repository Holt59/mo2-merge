cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake_common/mo2.cmake)

project(modorganizer2)

set(USE_FOLDERS TRUE)

add_subdirectory(uibase)

# set(CMAKE_FOLDER python)
# add_subdirectory(python)
# unset(CMAKE_FOLDER)

set(CMAKE_FOLDER libs)
file(GLOB library_directories
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    LIST_DIRECTORIES TRUE
    "libs/*")
foreach(library_directory ${library_directories})
    add_subdirectory(${library_directory})
endforeach()
unset(CMAKE_FOLDER)

# set(CMAKE_FOLDER plugins)
# file(GLOB plugin_directories
#     RELATIVE ${CMAKE_CURRENT_LIST_DIR}
#     LIST_DIRECTORIES TRUE
#     "plugins/*")
# foreach(plugin_directory ${plugin_directories})
#     add_subdirectory(${plugin_directory})
# endforeach()
# unset(CMAKE_FOLDER)

set(CMAKE_FOLDER tools)
file(GLOB tool_directories
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    LIST_DIRECTORIES TRUE
    "tools/*")
foreach(tool_directory ${tool_directories})
    add_subdirectory(${tool_directory})
endforeach()
unset(CMAKE_FOLDER)

add_subdirectory(modorganizer)
