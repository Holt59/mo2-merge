cmake_minimum_required(VERSION 3.16)

find_package(7zip CONFIG REQUIRED)

add_library(archive SHARED)
mo2_configure_library(archive WARNINGS OFF)
target_link_libraries(archive PRIVATE 7zip::7zip)
target_compile_definitions(archive PRIVATE -DMODORGANIZER_ARCHIVE_BUILDING)
mo2_install_target(archive)
