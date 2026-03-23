cmake_minimum_required(VERSION 3.14)

add_library(pegium_example_statemachine
    src/StatemachineModule.cpp
    src/cli/Generator.cpp
    src/lsp/StatemachineFormatter.cpp
    src/validation/StatemachineValidator.cpp
)

target_include_directories(pegium_example_statemachine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_statemachine PUBLIC cxx_std_20)
target_link_libraries(pegium_example_statemachine PUBLIC pegium)

add_executable(pegium-example-statemachine-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-statemachine-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-statemachine-cli PRIVATE
    PegiumCli
    pegium_example_statemachine
)

add_executable(pegium-example-statemachine-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-statemachine-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-statemachine-lsp PRIVATE pegium_example_statemachine)
