add_library(pegium_example_requirements_core
    src/core/Language.cpp
    src/core/RequirementsModule.cpp
    src/core/validation/RequirementsValidator.cpp
    src/core/validation/TestsValidator.cpp
)

target_include_directories(pegium_example_requirements_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_requirements_core PUBLIC cxx_std_20)
target_link_libraries(pegium_example_requirements_core
    PUBLIC
        pegium::core
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_requirements_lsp
    src/lsp/RequirementsModule.cpp
    src/lsp/RequirementsFormatter.cpp
)

target_include_directories(pegium_example_requirements_lsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_requirements_lsp PUBLIC cxx_std_20)
target_link_libraries(pegium_example_requirements_lsp
    PUBLIC
        pegium_example_requirements_core
        pegium::lsp
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_requirements_cli
    src/cli/CliUtils.cpp
    src/cli/Generator.cpp
)

target_include_directories(pegium_example_requirements_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_requirements_cli PUBLIC cxx_std_20)
target_link_libraries(pegium_example_requirements_cli
    PUBLIC
        pegium_example_requirements_core
    PRIVATE
        pegium::cli
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-requirements-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-requirements-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-requirements-cli PRIVATE
    pegium::cli
    pegium_example_requirements_cli
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-requirements-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-requirements-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-requirements-lsp PRIVATE
    pegium_example_requirements_lsp
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

set_property(GLOBAL APPEND PROPERTY PEGIUM_EXAMPLE_LANGUAGE_TARGETS
    pegium_example_requirements_core
)

if(BUILD_TESTING)
    file(GLOB_RECURSE PEGIUM_EXAMPLE_REQUIREMENTS_TEST_SOURCES CONFIGURE_DEPENDS
        tests/*.cpp
    )

    add_executable(pegium-example-requirements-test
        ${PEGIUM_EXAMPLE_REQUIREMENTS_TEST_SOURCES}
    )
    target_compile_features(pegium-example-requirements-test PRIVATE cxx_std_20)
    target_compile_definitions(pegium-example-requirements-test PRIVATE
        PEGIUM_EXAMPLE_REQUIREMENTS_CLI_PATH="$<TARGET_FILE:pegium-example-requirements-cli>"
    )
    target_link_libraries(pegium-example-requirements-test PRIVATE
        pegium_example_requirements_core
        pegium_example_requirements_lsp
        pegium_example_requirements_cli
        pegium_test_support_examples
        GTest::gtest_main
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
    )
    add_dependencies(pegium-example-requirements-test
        pegium-example-requirements-cli
    )
    pegium_register_gtest_target(pegium-example-requirements-test LABELS
        example
        requirements
    )
endif()
