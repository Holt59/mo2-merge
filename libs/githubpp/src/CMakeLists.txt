cmake_minimum_required(VERSION 3.16)

add_library(githubpp STATIC)
mo2_configure_library(githubpp WARNINGS OFF AUTOMOC ON)
target_link_libraries(githubpp PRIVATE Qt6::Network)
mo2_install_target(githubpp)
