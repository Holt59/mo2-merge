set(VCPKG_USE_HEAD_VERSION ON)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ModOrganizer2/usvfs
    HEAD_REF dev/cmake
)

set(VCPKG_TARGET_ARCHITECTURE x86)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    WINDOWS_USE_MSBUILD
)
vcpkg_cmake_install()

# set(VCPKG_TARGET_ARCHITECTURE x86)
# vcpkg_cmake_configure(
#     SOURCE_PATH "${SOURCE_PATH}"
#     WINDOWS_USE_MSBUILD
# )
# vcpkg_cmake_install()
