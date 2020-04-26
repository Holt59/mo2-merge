cmake_minimum_required(VERSION 3.16)

project(installer_quick)
set(project_type plugin)
set(enable_warnings OFF)

include(../cmake_common/project.cmake)
add_subdirectory(src)
