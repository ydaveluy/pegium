cmake_minimum_required(VERSION 3.14)

file(GLOB_RECURSE PEGIUM_ALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set(PEGIUM_LSP_SOURCES ${PEGIUM_ALL_SOURCES})
list(FILTER PEGIUM_LSP_SOURCES INCLUDE REGEX "/lsp/")
list(APPEND PEGIUM_LSP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/services/Services.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/services/SharedServices.cpp"
)

set(PEGIUM_CORE_SOURCES ${PEGIUM_ALL_SOURCES})
foreach(source IN LISTS PEGIUM_LSP_SOURCES)
    list(REMOVE_ITEM PEGIUM_CORE_SOURCES "${source}")
endforeach()

set(PEGIUM_LIBRARY_TYPE SHARED)
if(PEGIUM_SANITIZERS_ENABLED)
    set(PEGIUM_LIBRARY_TYPE STATIC)
endif()

add_library(PegiumCore ${PEGIUM_LIBRARY_TYPE} ${PEGIUM_CORE_SOURCES})
add_library(Pegium ${PEGIUM_LIBRARY_TYPE} ${PEGIUM_LSP_SOURCES})
add_library(pegium ALIAS Pegium)

target_include_directories(PegiumCore
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(Pegium
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(PegiumCore PUBLIC cxx_std_20)
target_compile_features(Pegium PUBLIC cxx_std_20)

target_compile_options(PegiumCore PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-visibility-inlines-hidden>
)
target_compile_options(Pegium PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-visibility-inlines-hidden>
)

target_link_libraries(Pegium PUBLIC PegiumCore lsp::lsp)
