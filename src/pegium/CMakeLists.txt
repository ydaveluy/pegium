cmake_minimum_required(VERSION 3.14)

add_subdirectory(core)
add_subdirectory(converters)
add_subdirectory(lsp)
add_subdirectory(cli)

add_library(pegium ALIAS Pegium)
