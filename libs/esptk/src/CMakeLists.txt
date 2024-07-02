cmake_minimum_required(VERSION 3.16)

find_package(Boost CONFIG REQUIRED)

add_library(esptk STATIC)
mo2_configure_library(esptk)
target_link_libraries(esptk PRIVATE Boost::headers)
mo2_install_target(esptk)
