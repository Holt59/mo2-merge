cmake_minimum_required(VERSION 3.16)

find_package(Boost REQUIRED CONFIG COMPONENTS thread interprocess assign)
find_package(ZLIB REQUIRED)
find_package(lz4 CONFIG REQUIRED)

add_library(bsatk STATIC)
mo2_configure_library(bsatk WARNINGS OFF PERMISSIVE ON)
target_link_libraries(bsatk
    PUBLIC Boost::headers Boost::thread
    PRIVATE ZLIB::ZLIB lz4::lz4 Boost::interprocess Boost::assign
)
mo2_install_target(bsatk)
