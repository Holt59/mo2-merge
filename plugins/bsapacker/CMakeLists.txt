cmake_minimum_required(VERSION 3.16)

project(bsa_packer)
add_subdirectory(src)

set(BSAPACKER_TESTS ${BSAPACKER_TESTS} CACHE BOOL "build tests for bsapacker plugin")
if(BSAPACKER_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
