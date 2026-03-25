cmake_minimum_required(VERSION 3.14)

project(pegium_fetchcontent_consumer LANGUAGES CXX)

if(NOT DEFINED PEGIUM_SOURCE_DIR)
    message(FATAL_ERROR "PEGIUM_SOURCE_DIR must be set.")
endif()

include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "")
set(PEGIUM_BUILD_EXAMPLES OFF CACHE BOOL "")
set(PEGIUM_BUILD_FUZZ_TARGETS OFF CACHE BOOL "")

FetchContent_Declare(
    pegium
    SOURCE_DIR "${PEGIUM_SOURCE_DIR}"
)
FetchContent_MakeAvailable(pegium)

foreach(targetName IN ITEMS PegiumCore PegiumConverters PegiumLsp PegiumCli)
    get_target_property(targetExcluded ${targetName} EXCLUDE_FROM_ALL)
    if(NOT targetExcluded)
        message(FATAL_ERROR
            "${targetName} must be EXCLUDE_FROM_ALL when Pegium is consumed as a subproject."
        )
    endif()
endforeach()

foreach(targetName IN ITEMS pegium::core pegium::converters pegium::lsp pegium::cli)
    if(NOT TARGET ${targetName})
        message(FATAL_ERROR "Expected public target '${targetName}' to exist.")
    endif()
endforeach()

add_library(pegium_fetchcontent_core INTERFACE)
target_link_libraries(pegium_fetchcontent_core INTERFACE pegium::core)

add_library(pegium_fetchcontent_converters INTERFACE)
target_link_libraries(pegium_fetchcontent_converters INTERFACE pegium::converters)

add_library(pegium_fetchcontent_lsp INTERFACE)
target_link_libraries(pegium_fetchcontent_lsp INTERFACE pegium::lsp)

add_library(pegium_fetchcontent_cli INTERFACE)
target_link_libraries(pegium_fetchcontent_cli INTERFACE pegium::cli)
