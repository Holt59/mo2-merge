cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0144 NEW)

project(plugin_python CXX)

find_package(Python 3.12 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

add_custom_target(PyQt6)
mo2_python_pip_install(PyQt6
	DIRECTORY ${CMAKE_BINARY_DIR}/pylibs/
	PACKAGES
		PyQt${QT_MAJOR_VERSION}==${QT_VERSION}
		sip==6.8.5)

# set(PYTHON_BUILD_PATH ${PYTHON_ROOT}/PCBuild/amd64)

# # find Python - lots of "Hints" since we have a weird setup
# set(Python_USE_STATIC_LIBS False)
# set(Python_INCLUDE_DIR ${PYTHON_ROOT}/Include)
# set(Python_EXECUTABLE ${PYTHON_BUILD_PATH}/python.exe)
# if (EXISTS "${PYTHON_BUILD_PATH}/python_d.exe")
# 	set(Python_EXECUTABLE ${PYTHON_BUILD_PATH}/python_d.exe)
# endif()
# file(GLOB Python_LIBRARY ${PYTHON_BUILD_PATH}/python[0-9][0-9]*.lib)
# find_package(Python COMPONENTS Interpreter Development REQUIRED)

# # pybind11 needs uppercase (at least EXECUTABLE and LIBRARY)
# set(PYTHON_EXECUTABLE ${Python_EXECUTABLE})
# set(PYTHON_INCLUDE_DIR ${Python_INCLUDE_DIR})
# set(PYTHON_LIBRARY ${Python_LIBRARY})

# # useful for naming DLL, zip, etc. (3.10 -> 310)
# set(Python_VERSION_SHORT ${Python_VERSION_MAJOR}${Python_VERSION_MINOR})

# projects
add_subdirectory(src)

# tests (if requested)
set(PLUGIN_PYTHON_TESTS ${PLUGIN_PYTHON_TESTS} CACHE BOOL "build tests for plugin_python")
if (PLUGIN_PYTHON_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
