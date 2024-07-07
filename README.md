# ModOrganizer 2

## How to build?

### Requirements

#### Visual Studio

- Install Visual Studio 2022 (Installer)
  - Desktop development with C++
  - Desktop .NET desktop development (needed by OMOD and FOMOD installers)
  - Individual Components:
    - .Net Framework 4.8 SDK
    - .Net Framework 4.7.2 targeting pack (OMOD targets 4.8 but VS still requires the package for other .Net components)
    - Windows Universal C Runtime
    - C++ ATL for latest v143 build Tools (x86 & x64)
    - C++ /CLI support for v143 build Tools (Latest) (for OMOD and FOMOD installers)
    - Windows 11 SDK (get latest)
    - C++ Build Tools core features
    - Git for Windows (Skip if you have this already installed outside of the VS installer)
    - CMake tools for Windows (Skip if you have this already installed outside of the VS installer)

#### VCPKG

- Install VCPKG
  - Clone [https://github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg)
  - Run `bootstrap-vcpkg.bat -disableMetrics` from the clone repository
  - Set the environment variable `VCPKG_ROOT` to the cloned repository

#### Python

- Install latest Python 3.12 from [https://www.python.org/](https://www.python.org/)

### Building

```pwsh
# build USVFS
.\build-usvfs.ps1

# build MO2
# - MO2 will be installed under ${CMAKE_INSTALL_PREFIX}/bin
#
# if CMake fails to find the right Python, you can set -DPython_ROOT_DIR with the path
# to your Python installation
#
cmake --preset vs2022-windows "-DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64"

# build and install MO2
cmake --build vsbuild --config RelWithDebInfo --target INSTALL
```
