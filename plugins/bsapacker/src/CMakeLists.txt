cmake_minimum_required(VERSION 3.16)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Concurrent)
find_path(BEXT_DI_INCLUDE_DIRS "boost/di.hpp")
find_package(libbsarch CONFIG REQUIRED)

add_library(bsa_packer SHARED)
mo2_configure_plugin(bsa_packer WARNINGS OFF)
target_link_libraries(bsa_packer PRIVATE Qt6::Concurrent libbsarch::libbsarch)
target_include_directories(bsa_packer PRIVATE ${BEXT_DI_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(bsa_packer PRIVATE BSAPACKER_LIBRARY)
mo2_install_target(bsa_packer)

install(FILES $<TARGET_FILE:libbsarch::libbsarch> DESTINATION bin/dlls)
