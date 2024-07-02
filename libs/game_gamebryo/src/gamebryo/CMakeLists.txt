cmake_minimum_required(VERSION 3.16)

find_package(ZLIB REQUIRED)
find_package(lz4 CONFIG REQUIRED)

add_library(game_gamebryo STATIC)
mo2_configure_library(game_gamebryo WARNINGS OFF TRANSLATIONS ON AUTOMOC ON)
target_link_libraries(game_gamebryo
	PUBLIC uibase
	PRIVATE ZLIB::ZLIB lz4::lz4
)
mo2_install_target(game_gamebryo)
