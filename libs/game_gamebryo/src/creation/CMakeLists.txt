cmake_minimum_required(VERSION 3.16)

find_package(lz4 CONFIG REQUIRED)

add_library(game_creation STATIC)
mo2_configure_library(game_creation
	WARNINGS OFF
	TRANSLATIONS ON)
target_link_libraries(game_creation PUBLIC game_gamebryo PRIVATE lz4::lz4)
mo2_install_target(game_creation)
