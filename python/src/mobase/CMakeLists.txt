cmake_minimum_required(VERSION 3.16)

pybind11_add_module(mobase MODULE)
mo2_configure_library(mobase
	SOURCE_TREE
	WARNINGS 4
	EXTERNAL_WARNINGS 4
	AUTOMOC ON
	TRANSLATIONS OFF
)
target_link_libraries(mobase PRIVATE uibase pybind11::qt pybind11::utils)
