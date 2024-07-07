cmake_minimum_required(VERSION 3.22)

include(FetchContent)

project(explorer++ NONE)

FetchContent_Declare(
    explorerpp
    URL  "https://download.explorerplusplus.com/stable/1.4.0/explorerpp_x64.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(explorerpp)

install(DIRECTORY ${explorerpp_SOURCE_DIR}/ DESTINATION bin/explorer++/)
