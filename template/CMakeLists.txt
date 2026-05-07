cmake_minimum_required(VERSION 3.14)
project(mydsl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MYDSL_BUILD_LSP "Build the LSP server" ON)
option(MYDSL_BUILD_TESTS "Build the smoke test" ON)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/pegium.cmake)

# --- core library --------------------------------------------------------
add_library(mydsl_core
    src/mydsl/core/Module.cpp
)
target_include_directories(mydsl_core PUBLIC src)
target_compile_features(mydsl_core PUBLIC cxx_std_20)
target_link_libraries(mydsl_core PUBLIC pegium::core)

# --- CLI executable ------------------------------------------------------
add_executable(mydsl-cli
    src/mydsl/cli_main.cpp
)
target_link_libraries(mydsl-cli PRIVATE mydsl_core pegium::cli)

# --- LSP executable (optional) -------------------------------------------
if(MYDSL_BUILD_LSP)
    add_library(mydsl_lsp
        src/mydsl/lsp/Module.cpp
    )
    target_include_directories(mydsl_lsp PUBLIC src)
    target_compile_features(mydsl_lsp PUBLIC cxx_std_20)
    target_link_libraries(mydsl_lsp PUBLIC mydsl_core pegium::lsp)

    add_executable(mydsl-lsp
        src/mydsl/lsp_main.cpp
    )
    target_link_libraries(mydsl-lsp PRIVATE mydsl_lsp)
endif()

# --- smoke test ----------------------------------------------------------
if(MYDSL_BUILD_TESTS)
    enable_testing()
    add_executable(mydsl-smoke
        tests/ParseSmoke.cpp
    )
    target_compile_definitions(mydsl-smoke PRIVATE
        MYDSL_SAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/example/sample.mydsl"
    )
    target_link_libraries(mydsl-smoke PRIVATE mydsl_core pegium::cli)
    add_test(NAME mydsl-smoke COMMAND mydsl-smoke)
endif()
