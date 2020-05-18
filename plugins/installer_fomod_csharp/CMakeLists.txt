cmake_minimum_required(VERSION 3.16)

project(installer_fomod_csharp)
set(project_type plugin)
set(enable_warnings OFF)

add_compile_options("/Zc:twoPhase-")

if(DEFINED DEPENDENCIES_DIR)
	include(${DEPENDENCIES_DIR}/modorganizer_super/cmake_common/project.cmake)
else()
	include(../cmake_common/project.cmake)
endif()
add_subdirectory(src)
set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	COMMON_LANGUAGE_RUNTIME "")