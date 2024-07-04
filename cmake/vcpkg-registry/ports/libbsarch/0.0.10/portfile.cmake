vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/ModOrganizer2/libbsarch/releases/download/${VERSION}/libbsarch-${VERSION}-release-x64.7z"
    FILENAME "libbsarch-${VERSION}-release-x64.7z"
    SHA512 43ACE279D9A245B094552760F290EECBAAFAE8C5EF96BCFA77CF8457383B4C802A75258A72D6CABE60EB100ACDC3DA22E975085349B4B37D834534CD86E31DB3
)
vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
    NO_REMOVE_ONE_LEVEL
)

file(INSTALL
    ${SOURCE_PATH}/utils
    ${SOURCE_PATH}/base_types.hpp
    ${SOURCE_PATH}/base_types.hpp
    ${SOURCE_PATH}/bs_archive_auto.hpp
    ${SOURCE_PATH}/bs_archive_entries.h
    ${SOURCE_PATH}/bs_archive.h
    ${SOURCE_PATH}/libbsarch.h
    ${SOURCE_PATH}/libbsarch.hpp
    DESTINATION ${CURRENT_PACKAGES_DIR}/include)
file(INSTALL ${SOURCE_PATH}/libbsarch.dll DESTINATION ${CURRENT_PACKAGES_DIR}/bin)
file(INSTALL
    ${SOURCE_PATH}/libbsarch.lib
    ${SOURCE_PATH}/libbsarch.pdb
    ${SOURCE_PATH}/libbsarch_OOP.lib
    ${SOURCE_PATH}/libbsarch_OOP.pdb
    DESTINATION ${CURRENT_PACKAGES_DIR}/lib)

vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/ModOrganizer2/libbsarch/releases/download/${VERSION}/libbsarch-${VERSION}-debug-x64.7z"
    FILENAME "libbsarch-${VERSION}-release-x64.7z"
    SHA512 3602C9DECE25380A54AE00BBBF1662C92014E24E205AB1E922C80E9A3DED6861DFD93390B2E265E35A68B4F44C00EE0F5ECE6F27E2B286F4892AE76FFA18E39C
)
vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH_DEBUG
    ARCHIVE ${ARCHIVE}
    NO_REMOVE_ONE_LEVEL
)

file(INSTALL ${SOURCE_PATH}/libbsarch.dll DESTINATION ${CURRENT_PACKAGES_DIR}/debug/bin)
file(INSTALL
    ${SOURCE_PATH}/libbsarch.lib
    ${SOURCE_PATH}/libbsarch.pdb
    ${SOURCE_PATH}/libbsarch_OOP.lib
    ${SOURCE_PATH}/libbsarch_OOP.pdb
    DESTINATION ${CURRENT_PACKAGES_DIR}/debug/lib)

# dds.h is not part of DirectXTex
vcpkg_download_distfile(ARCHIVE
    URLS "https://raw.githubusercontent.com/microsoft/DirectXTex/e102d0bd3e1a9e59e9aa7276b3ff27c484b783b6/DirectXTex/DDS.h"
    FILENAME "DDS.h"
    SHA512 49ED2C454A39D59EBAE06F2E96E814246754C83ECE074E052A8872F7E16FA7C9E00FEC3B1F1757E09AE9FC1D7C7AFCF7C845D377A3E8EC876E7F7D5CEDB1487A
)
file(INSTALL ${ARCHIVE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)

file(INSTALL ${CMAKE_CURRENT_LIST_DIR}/libbsarchConfig.cmake DESTINATION ${CURRENT_PACKAGES_DIR}/share/libbsarch)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CURRENT_PACKAGES_DIR}/share/libbsarch/libbsarchConfigVersion.cmake"
  VERSION "${VERSION}"
  COMPATIBILITY AnyNewerVersion
)
