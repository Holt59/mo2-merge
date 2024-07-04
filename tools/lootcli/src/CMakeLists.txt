cmake_minimum_required(VERSION 3.16)

# this disable a warning for libloot where IMPORTED_LOCATION is missing for some
# configuration that we do not use
if (POLICY CMP0111)
    cmake_policy(SET CMP0111 OLD)
endif()

find_package(tomlplusplus CONFIG REQUIRED)
find_package(libloot CONFIG REQUIRED)

find_package(Boost REQUIRED CONFIG COMPONENTS locale)
find_package(Qt6 REQUIRED)

add_executable(lootcli WIN32)

mo2_configure_executable(lootcli WARNINGS OFF TRANSLATIONS OFF)
target_link_libraries(lootcli
    PRIVATE Boost::headers Boost::locale Qt6::Core tomlplusplus::tomlplusplus libloot::loot)
target_include_directories(lootcli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

mo2_install_target(lootcli INSTALLDIR bin/loot)
get_target_property(LIBLOOT_DLL libloot::loot IMPORTED_LOCATION_RELWITHDEBINFO)
INSTALL(FILES ${LIBLOOT_DLL} DESTINATION bin/loot/)
