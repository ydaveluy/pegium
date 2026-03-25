add_library(pegium_example_domainmodel_core
    src/core/DomainModelModule.cpp
    src/core/references/DomainModelScopeComputation.cpp
    src/core/references/QualifiedNameProvider.cpp
    src/core/validation/DomainModelValidator.cpp
)

target_include_directories(pegium_example_domainmodel_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_domainmodel_core PUBLIC cxx_std_20)
target_link_libraries(pegium_example_domainmodel_core
    PUBLIC
        pegium::core
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_domainmodel_lsp
    src/lsp/DomainModelModule.cpp
    src/lsp/DomainModelFormatter.cpp
    src/lsp/DomainModelRenameProvider.cpp
)

target_include_directories(pegium_example_domainmodel_lsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_domainmodel_lsp PUBLIC cxx_std_20)
target_link_libraries(pegium_example_domainmodel_lsp
    PUBLIC
        pegium_example_domainmodel_core
        pegium::lsp
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_domainmodel_cli
    src/cli/CliUtils.cpp
    src/cli/Generator.cpp
)

target_include_directories(pegium_example_domainmodel_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_domainmodel_cli PUBLIC cxx_std_20)
target_link_libraries(pegium_example_domainmodel_cli
    PUBLIC
        pegium_example_domainmodel_core
    PRIVATE
        pegium::cli
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-domainmodel-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-domainmodel-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-domainmodel-cli PRIVATE
    pegium::cli
    pegium_example_domainmodel_cli
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-domainmodel-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-domainmodel-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-domainmodel-lsp PRIVATE
    pegium_example_domainmodel_lsp
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

set_property(GLOBAL APPEND PROPERTY PEGIUM_EXAMPLE_LANGUAGE_TARGETS
    pegium_example_domainmodel_core
)

if(BUILD_TESTING)
    file(GLOB_RECURSE PEGIUM_EXAMPLE_DOMAINMODEL_TEST_SOURCES CONFIGURE_DEPENDS
        tests/*.cpp
    )

    add_executable(pegium-example-domainmodel-test
        ${PEGIUM_EXAMPLE_DOMAINMODEL_TEST_SOURCES}
    )
    target_compile_features(pegium-example-domainmodel-test PRIVATE cxx_std_20)
    target_compile_definitions(pegium-example-domainmodel-test PRIVATE
        PEGIUM_EXAMPLE_DOMAINMODEL_CLI_PATH="$<TARGET_FILE:pegium-example-domainmodel-cli>"
    )
    target_link_libraries(pegium-example-domainmodel-test PRIVATE
        pegium::cli
        pegium_example_domainmodel_core
        pegium_example_domainmodel_lsp
        pegium_example_domainmodel_cli
        pegium_test_support_examples
        GTest::gtest_main
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
    )
    add_dependencies(pegium-example-domainmodel-test
        pegium-example-domainmodel-cli
    )
    pegium_register_gtest_target(pegium-example-domainmodel-test LABELS
        example
        domainmodel
    )
endif()
