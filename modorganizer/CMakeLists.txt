cmake_minimum_required(VERSION 3.16)

# TODO: move these to cmake_common?
set(OPENSSL_USE_STATIC_LIBS FALSE CACHE STRING "" FORCE)
set(MySQL_INCLUDE_DIRS CACHE STRING "" FORCE)

project(organizer)

add_subdirectory(src)

# install USVFS
find_package(usvfs REQUIRED)

install(
FILES
    # DLLs
    $<TARGET_FILE:usvfs_x86::usvfs_dll>
    $<TARGET_FILE:usvfs_x64::usvfs_dll>
    # Proxies
    $<TARGET_FILE:usvfs_x86::usvfs_proxy>
    $<TARGET_FILE:usvfs_x64::usvfs_proxy>
DESTINATION bin)

install(
FILES
    # Libs
    $<TARGET_LINKER_FILE:usvfs_x86::usvfs_dll>
    $<TARGET_LINKER_FILE:usvfs_x64::usvfs_dll>
DESTINATION libs)

install(
FILES
    # PDBs - kind of a hack since there is no way to find the PDB with CMake currently
    $<TARGET_LINKER_FILE_DIR:usvfs_x86::usvfs_dll>/usvfs_x86.pdb
    $<TARGET_LINKER_FILE_DIR:usvfs_x86::usvfs_dll>/usvfs_x64.pdb
    $<TARGET_LINKER_FILE_DIR:usvfs_x86::usvfs_dll>/usvfs_proxy_x86.pdb
    $<TARGET_LINKER_FILE_DIR:usvfs_x86::usvfs_dll>/usvfs_proxy_x64.pdb
DESTINATION pdb
OPTIONAL
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dump_running_process.bat DESTINATION bin)
