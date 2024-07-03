# USVFS contains submodule so it's a bit of hack to download it properly
find_program(GIT git)

set(GIT_URL "https://github.com/ModOrganizer2/usvfs.git")
set(GIT_REV "master")

if(NOT EXISTS "${DOWNLOADS}/usvfs.git")
    message(STATUS "Cloning ${GIT_URL} into ${DOWNLOADS}/usvfs.git...")
    vcpkg_execute_required_process(
        COMMAND ${GIT} clone --bare ${GIT_URL} ${DOWNLOADS}/usvfs.git
        WORKING_DIRECTORY ${DOWNLOADS}
        LOGNAME clone
    )
endif()

set(SOURCE_PATH ${CURRENT_BUILDTREES_DIR}/master/src)

if(NOT EXISTS "${SOURCE_PATH}/.git")
    message(STATUS "Adding worktree... ")
    vcpkg_execute_required_process(
        COMMAND ${GIT} worktree add -f --detach ${SOURCE_PATH} ${GIT_REV}
        WORKING_DIRECTORY ${DOWNLOADS}/usvfs.git
        LOGNAME worktree
    )
endif()

vcpkg_execute_required_process(
    COMMAND ${GIT} pull
    WORKING_DIRECTORY ${SOURCE_PATH}
    LOGNAME pull
)
vcpkg_execute_required_process(
    COMMAND ${GIT} submodule update --init
    WORKING_DIRECTORY ${SOURCE_PATH}
    LOGNAME submodule
)

vcpkg_msbuild_install(
    SOURCE_PATH ${SOURCE_PATH}
    PROJECT_SUBPATH vsbuild/usvfs.sln
    NO_INSTALL
    RELEASE_CONFIGURATION Release
    DEBUG_CONFIGURATION Debug
    TARGET usvfs_proxy
    PLATFORM x64
    OPTIONS -nologo -maxCpuCount
        /p:UseMultiToolTask=true
        /p:EnforceProcessCountAcrossBuilds=true
        /p:PlatformToolset=v143
        /p:WindowsTargetPlatformVersion=10.0.22621.0
        /p:RunCodeAnalysis=false
)

# vcpkg_extract_source_archive_ex(
#     OUT_SOURCE_PATH SOURCE_PATH
#     ARCHIVE ${ARCHIVE}
# )

# file(INSTALL ${SOURCE_PATH}/include DESTINATION ${CURRENT_PACKAGES_DIR})
# file(INSTALL ${SOURCE_PATH}/bin DESTINATION ${CURRENT_PACKAGES_DIR})
# file(INSTALL ${SOURCE_PATH}/lib DESTINATION ${CURRENT_PACKAGES_DIR})

# file(INSTALL ${SOURCE_PATH}/bin DESTINATION ${CURRENT_PACKAGES_DIR}/debug)
# file(INSTALL ${SOURCE_PATH}/lib DESTINATION ${CURRENT_PACKAGES_DIR}/debug)

# vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")

# vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
