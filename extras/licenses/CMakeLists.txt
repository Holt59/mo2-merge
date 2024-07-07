cmake_minimum_required(VERSION 3.22)

file(GLOB licenses "*.txt")
install(FILES ${licenses} DESTINATION bin/licenses/)
