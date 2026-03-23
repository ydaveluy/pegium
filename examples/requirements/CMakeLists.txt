cmake_minimum_required(VERSION 3.14)

add_library(pegium_example_requirements
    src/cli/CliUtils.cpp
    src/RequirementsModule.cpp
    src/cli/Generator.cpp
    src/Language.cpp
    src/lsp/RequirementsFormatter.cpp
    src/validation/RequirementsValidator.cpp
    src/validation/TestsValidator.cpp
)

target_include_directories(pegium_example_requirements
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_requirements PUBLIC cxx_std_20)
target_link_libraries(pegium_example_requirements PUBLIC
    pegium
    PegiumCli
)

add_executable(pegium-example-requirements-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-requirements-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-requirements-cli PRIVATE
    PegiumCli
    pegium_example_requirements
)

add_executable(pegium-example-requirements-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-requirements-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-requirements-lsp PRIVATE pegium_example_requirements)
