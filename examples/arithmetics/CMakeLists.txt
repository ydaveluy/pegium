add_library(pegium_example_arithmetics_core
    src/core/ArithmeticsModule.cpp
    src/core/Language.cpp
    src/core/validation/ArithmeticsValidator.cpp
)

target_include_directories(pegium_example_arithmetics_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_arithmetics_core PUBLIC cxx_std_20)
target_link_libraries(pegium_example_arithmetics_core
    PUBLIC
        pegium::core
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_arithmetics_lsp
    src/lsp/ArithmeticsModule.cpp
    src/lsp/ArithmeticsCodeActionProvider.cpp
    src/lsp/ArithmeticsFormatter.cpp
)

target_include_directories(pegium_example_arithmetics_lsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_arithmetics_lsp PUBLIC cxx_std_20)
target_link_libraries(pegium_example_arithmetics_lsp
    PUBLIC
        pegium_example_arithmetics_core
        pegium::lsp
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_arithmetics_cli
    src/cli/Evaluator.cpp
)

target_include_directories(pegium_example_arithmetics_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_arithmetics_cli PUBLIC cxx_std_20)
target_link_libraries(pegium_example_arithmetics_cli
    PUBLIC
        pegium_example_arithmetics_core
    PRIVATE
        pegium::cli
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-arithmetics-cli
    src/cli_main.cpp
)
target_include_directories(pegium-example-arithmetics-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(pegium-example-arithmetics-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-arithmetics-cli PRIVATE
    pegium::cli
    pegium_example_arithmetics_cli
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-arithmetics-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-arithmetics-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-arithmetics-lsp PRIVATE
    pegium_example_arithmetics_lsp
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

set_property(GLOBAL APPEND PROPERTY PEGIUM_EXAMPLE_LANGUAGE_TARGETS
    pegium_example_arithmetics_core
)

if(BUILD_TESTING)
    file(GLOB_RECURSE PEGIUM_EXAMPLE_ARITHMETICS_TEST_SOURCES CONFIGURE_DEPENDS
        tests/*.cpp
    )

    add_executable(pegium-example-arithmetics-test
        ${PEGIUM_EXAMPLE_ARITHMETICS_TEST_SOURCES}
    )
    target_compile_features(pegium-example-arithmetics-test PRIVATE cxx_std_20)
    target_compile_definitions(pegium-example-arithmetics-test PRIVATE
        PEGIUM_EXAMPLE_ARITHMETICS_CLI_PATH="$<TARGET_FILE:pegium-example-arithmetics-cli>"
    )
    target_link_libraries(pegium-example-arithmetics-test PRIVATE
        pegium_example_arithmetics_core
        pegium_example_arithmetics_lsp
        pegium_example_arithmetics_cli
        pegium_test_support_examples
        GTest::gtest_main
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
    )
    add_dependencies(pegium-example-arithmetics-test
        pegium-example-arithmetics-cli
    )
    pegium_register_gtest_target(pegium-example-arithmetics-test LABELS
        example
        arithmetics
    )
    add_test(
        NAME PegiumRecoveryCorpusArithmetics
        COMMAND $<TARGET_FILE:pegium-example-arithmetics-test>
            --gtest_filter=*RecoverySample*:*RecoveryProbe*
    )
    set_tests_properties(PegiumRecoveryCorpusArithmetics PROPERTIES
        LABELS "recovery-corpus"
    )
endif()
