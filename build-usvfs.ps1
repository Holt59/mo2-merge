git submodule update --init

cmake usvfs --preset vs2022-windows-x86 `
    "-DBUILD_TESTING=OFF" `
    "-DVCPKG_BUILD_TYPE=Release" `
    "-DCMAKE_INSTALL_PREFIX=.\usvfs\install"
cmake usvfs --preset vs2022-windows-x64 `
    "-DBUILD_TESTING=OFF" `
    "-DVCPKG_BUILD_TYPE=Release" `
    "-DCMAKE_INSTALL_PREFIX=.\usvfs\install"

cmake --build usvfs\vsbuild32 --config Release --target INSTALL
cmake --build usvfs\vsbuild64 --config Release --target INSTALL
