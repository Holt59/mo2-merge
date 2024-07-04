cmake_minimum_required(VERSION 3.16)

add_library(game_nehrim SHARED)
mo2_configure_plugin(game_nehrim WARNINGS OFF)
target_link_libraries(game_nehrim PRIVATE game_gamebryo)
mo2_install_target(game_nehrim)
