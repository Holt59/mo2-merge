cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Widgets)

add_library(pybind11-qt STATIC)
mo2_configure_library(pybind11-qt
	SOURCE_TREE
	WARNINGS 4
	EXTERNAL_WARNINGS 4
	AUTOMOC OFF
	TRANSLATIONS OFF
)
target_link_libraries(pybind11-qt PUBLIC pybind11::pybind11 Qt6::Core Qt6::Widgets)
target_include_directories(pybind11-qt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# this is kind of broken but it only works with this...
target_compile_definitions(pybind11-qt PUBLIC QT_NO_KEYWORDS)

add_library(pybind11::qt ALIAS pybind11-qt)
