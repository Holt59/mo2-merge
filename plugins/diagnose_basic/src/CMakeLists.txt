cmake_minimum_required(VERSION 3.16)

find_package(Boost CONFIG REQUIRED COMPONENTS assign graph)

add_library(diagnose_basic SHARED)
mo2_configure_plugin(diagnose_basic WARNINGS OFF)
target_link_libraries(diagnose_basic PRIVATE Boost::assign Boost::graph)
mo2_install_target(diagnose_basic)
