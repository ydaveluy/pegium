cmake_minimum_required(VERSION 3.14)

add_library(pegium_example_domainmodel
    src/DomainModelModule.cpp
    src/Language.cpp
    src/lsp/DomainModelFormatter.cpp
    src/lsp/DomainModelRenameProvider.cpp
    src/references/DomainModelScopeComputation.cpp
    src/references/QualifiedNameProvider.cpp
    src/validation/DomainModelValidator.cpp
)

target_include_directories(pegium_example_domainmodel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_domainmodel PUBLIC cxx_std_20)
target_link_libraries(pegium_example_domainmodel PUBLIC pegium)

add_executable(pegium-example-domainmodel-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-domainmodel-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-domainmodel-cli PRIVATE pegium_example_domainmodel)

add_executable(pegium-example-domainmodel-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-domainmodel-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-domainmodel-lsp PRIVATE pegium_example_domainmodel)
