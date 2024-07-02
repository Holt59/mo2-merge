cmake_minimum_required(VERSION 3.16)

# TODO: move these to cmake_common?
set(OPENSSL_USE_STATIC_LIBS FALSE CACHE STRING "" FORCE)
set(MySQL_INCLUDE_DIRS CACHE STRING "" FORCE)

project(organizer)
add_subdirectory(src)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION bin)

