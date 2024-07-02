cmake_minimum_required(VERSION 3.16)

project(uibase)

add_subdirectory(src)

set(UIBASE_TESTS ${UIBASE_TESTS} CACHE BOOL "build tests for uibase")
if (UIBASE_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

