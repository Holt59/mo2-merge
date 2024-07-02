cmake_minimum_required(VERSION 3.16)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 OLD)
endif()

mo2_find_tomlplusplus()
mo2_find_libloot()

find_package(Boost REQUIRED CONFIG)
find_package(Qt6 REQUIRED)

add_executable(lootcli WIN32)

mo2_configure_executable(lootcli WARNINGS OFF TRANSLATIONS OFF)
target_link_libraries(lootcli PRIVATE Boost::headers Qt6::Core tomlplusplus libloot)
target_include_directories(lootcli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

mo2_install_target(lootcli INSTALLDIR bin/loot)
