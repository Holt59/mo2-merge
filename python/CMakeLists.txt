cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0144 NEW)

project(plugin_python CXX)

find_package(Python 3.12 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

get_filename_component(Python_HOME ${Python_EXECUTABLE} PATH)
set(Python_DLL_DIR "${Python_HOME}\\DLLs")
set(Python_LIB_DIR "${Python_HOME}\\Lib")

add_custom_target(PyQt6)
mo2_python_pip_install(PyQt6
	DIRECTORY ${CMAKE_BINARY_DIR}/pylibs/
	PACKAGES
		PyQt${QT_MAJOR_VERSION}==${QT_VERSION}
		sip==6.8.5)

# useful for naming DLL, zip, etc. (3.10 -> 310)
set(Python_VERSION_SHORT ${Python_VERSION_MAJOR}${Python_VERSION_MINOR})

# projects
add_subdirectory(src)

# tests (if requested)
set(BUILD_TESTING ${BUILD_TESTING} CACHE BOOL "build tests for plugin_python")
if (BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()
