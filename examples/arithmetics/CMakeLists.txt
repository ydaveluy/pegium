cmake_minimum_required(VERSION 3.14)

add_library(pegium_example_arithmetics
    src/ArithmeticsModule.cpp
    src/lsp/ArithmeticsCodeActionProvider.cpp
    src/lsp/ArithmeticsFormatter.cpp
    src/Language.cpp
    src/references/ArithmeticsScopeProvider.cpp
    src/validation/ArithmeticsValidator.cpp
)

target_include_directories(pegium_example_arithmetics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_arithmetics PUBLIC cxx_std_20)
target_link_libraries(pegium_example_arithmetics PUBLIC pegium)

add_executable(pegium-example-arithmetics-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-arithmetics-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-arithmetics-cli PRIVATE pegium_example_arithmetics)

add_executable(pegium-example-arithmetics-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-arithmetics-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-arithmetics-lsp PRIVATE pegium_example_arithmetics)
