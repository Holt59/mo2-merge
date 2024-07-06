cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

cmake_policy(SET CMP0114 NEW)

ExternalProject_Add(usvfs_x86
    GIT_REPOSITORY https://github.com/ModOrganizer2/usvfs/
    GIT_TAG dev/cmake
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sources
    CMAKE_ARGS
        --preset vs2022-windows-x86
        -DBUILD_TESTING=OFF
        -UVCPKG_MANIFEST_FEATURES
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_LIST_DIR}
)
ExternalProject_Add_StepTargets(usvfs_x86 download install)

ExternalProject_Add(usvfs_x64
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sources
    CMAKE_ARGS
        --preset vs2022-windows-x64
        -DBUILD_TESTING=OFF
        -UVCPKG_MANIFEST_FEATURES
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_LIST_DIR}
)
ExternalProject_Add_StepDependencies(usvfs_x64 download usvfs_x86-download)
ExternalProject_Add_StepTargets(usvfs_x64 install)

add_library(usvfs::usvfs SHARED IMPORTED GLOBAL)
add_dependencies(usvfs::usvfs usvfs_x64-install usvfs_x86-install)
set_target_properties(usvfs::usvfs
    PROPERTIES
    IMPORTED_IMPLIB ${CMAKE_CURRENT_LIST_DIR}/lib/usvfs_x64.lib
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/bin/usvfs_x64.dll
)

# need to make directory first otherwise CMake is not happy... will be populated by
# external project
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(usvfs::usvfs INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

install(
FILES
    ${CMAKE_CURRENT_LIST_DIR}/bin/usvfs_x64.dll
    ${CMAKE_CURRENT_LIST_DIR}/bin/usvfs_x86.dll
    ${CMAKE_CURRENT_LIST_DIR}/bin/usvfs_proxy_x64.exe
    ${CMAKE_CURRENT_LIST_DIR}/bin/usvfs_proxy_x86.exe
DESTINATION bin)
