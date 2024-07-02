cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Core)

add_executable(helper)
mo2_configure_executable(helper ELEVATED  OFF TRANSLATIONS OFF)
target_link_libraries(helper PRIVATE Qt6::Core)
mo2_install_target(helper)
