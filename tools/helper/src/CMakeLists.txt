cmake_minimum_required(VERSION 3.16)

add_executable(helper)
mo2_configure_executable(helper ELEVATED  OFF TRANSLATIONS OFF)
target_link_libraries(helper PRIVATE Qt6::Core)
mo2_install_target(helper)
