cmake_minimum_required(VERSION 3.22)

include(FetchContent)

function(mo2_install_stylesheets URL)

    cmake_path(GET URL STEM URL_STEM)
    set(ss_name ss_${URL_STEM})
     string(TOLOWER ${ss_name} ss_name)
    message(STATUS "downloading ${ss_name} from ${URL}")

    FetchContent_Declare(
        ${ss_name}
        URL ${URL}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(${ss_name})

    if (NOT ${${ss_name}_POPULATED})
        message(FATAL_ERROR "failed to retrieve stylesheets from ${URL}")
    endif()

    install(DIRECTORY ${${ss_name}_SOURCE_DIR}/ DESTINATION bin/stylesheets/)
endfunction()

project(third-party-stylesheets NONE)

mo2_install_stylesheets("https://github.com/6788-00/paper-light-and-dark/releases/download/7.2/paper-light-and-dark.7z")
mo2_install_stylesheets("https://github.com/6788-00/paper-automata/releases/download/3.2/paper-automata.7z")
mo2_install_stylesheets("https://github.com/6788-00/paper-mono/releases/download/3.2/paper-mono.7z")
mo2_install_stylesheets("https://github.com/6788-00/1809-dark-mode/releases/download/3.0/1809.7z")
mo2_install_stylesheets("https://github.com/Trosski/ModOrganizer_Style_Morrowind/releases/download/1.1/Morrowind-MO2-Stylesheet.7z")
mo2_install_stylesheets("https://github.com/Trosski/Mod-Organizer-2-Skyrim-Stylesheet/releases/download/v1.1/Skyrim-MO2-Stylesheet.7z")
mo2_install_stylesheets("https://github.com/Trosski/ModOrganizer_Style_Fallout3/releases/download/v1.11/Fallout3-MO2-Stylesheet.7z")
mo2_install_stylesheets("https://github.com/Trosski/Mod-Organizer2-Fallout-4-Stylesheet/releases/download/v1.11/Fallout4-MO2-Stylesheet.7z")
mo2_install_stylesheets("https://github.com/Trosski/Starfield_MO2_Stylesheet/releases/download/V1.11/Starfield.MO2.Stylsheet.7z")
