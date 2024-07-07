# ModOrganizer 2

## How to build?

```pwsh
# build USVFS
.\build-usvfs.ps1

# build MO2 - (~41s without install)
# - MO2 will be installed under ${CMAKE_INSTALL_PREFIX}/bin
cmake --preset vs2022-windows "-DCMAKE_INSTALL_PREFIX=..\install" "-DQT_ROOT=C:\Qt\6.7.0\msvc2019_64\"


cmake --build vsbuild --config RelWithDebInfo --target INSTALL
