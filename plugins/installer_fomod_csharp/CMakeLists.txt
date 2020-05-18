cmake_minimum_required(VERSION 3.16)

set(CMAKE_INSTALL_PREFIX I:\\Projects\\CPP\\modorganizer-umbrella\\install)
set(DEPENDENCIES_DIR I:\\Projects\\CPP\\modorganizer-umbrella\\build)
set(BOOST_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\boost_1_73_0)
set(BOOST_LIBRARYDIR I:\\Projects\\CPP\\modorganizer-umbrella\\build\\boost_1_73_0\\lib64-msvc-14.2\\lib)
set(FMT_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\fmt-6.2.0)
set(SPDLOG_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\spdlog-v1.4.2)
set(LOOT_PATH I:\\Projects\\CPP\\modorganizer-umbrella\\build\\libloot-0.15.1-0-gf725dd7_0.15.1-win64)
set(LZ4_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\lz4-v1.9.2)
set(QT_ROOT I:\\Languages\\Qt\\5.14.2\\msvc2017_64)
set(ZLIB_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\zlib-1.2.11)
set(PYTHON_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\python-3.8.1)
set(SEVENZ_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\7zip-19.00)
set(LIBBSARCH_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\libbsarch-0.0.8-release-x64)
set(BOOST_DI_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\di)
set(GTEST_ROOT I:\\Projects\\CPP\\modorganizer-umbrella\\build\\googletest)

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