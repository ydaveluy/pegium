add_library(pegium_example_statemachine_core
    src/core/StatemachineModule.cpp
    src/core/validation/StatemachineValidator.cpp
)

target_include_directories(pegium_example_statemachine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_statemachine_core PUBLIC cxx_std_20)
target_link_libraries(pegium_example_statemachine_core
    PUBLIC
        pegium::core
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_statemachine_lsp
    src/lsp/StatemachineModule.cpp
    src/lsp/StatemachineFormatter.cpp
)

target_include_directories(pegium_example_statemachine_lsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_statemachine_lsp PUBLIC cxx_std_20)
target_link_libraries(pegium_example_statemachine_lsp
    PUBLIC
        pegium_example_statemachine_core
        pegium::lsp
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_library(pegium_example_statemachine_cli
    src/cli/Generator.cpp
)

target_include_directories(pegium_example_statemachine_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(pegium_example_statemachine_cli PUBLIC cxx_std_20)
target_link_libraries(pegium_example_statemachine_cli
    PUBLIC
        pegium_example_statemachine_core
    PRIVATE
        pegium::cli
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-statemachine-cli
    src/cli_main.cpp
)
target_compile_features(pegium-example-statemachine-cli PRIVATE cxx_std_20)
target_link_libraries(pegium-example-statemachine-cli PRIVATE
    pegium::cli
    pegium_example_statemachine_cli
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

add_executable(pegium-example-statemachine-lsp
    src/lsp_main.cpp
)
target_compile_features(pegium-example-statemachine-lsp PRIVATE cxx_std_20)
target_link_libraries(pegium-example-statemachine-lsp PRIVATE
    pegium_example_statemachine_lsp
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

set_property(GLOBAL APPEND PROPERTY PEGIUM_EXAMPLE_LANGUAGE_TARGETS
    pegium_example_statemachine_core
)

if(BUILD_TESTING)
    file(GLOB_RECURSE PEGIUM_EXAMPLE_STATEMACHINE_TEST_SOURCES CONFIGURE_DEPENDS
        tests/*.cpp
    )

    add_executable(pegium-example-statemachine-test
        ${PEGIUM_EXAMPLE_STATEMACHINE_TEST_SOURCES}
    )
    target_compile_features(pegium-example-statemachine-test PRIVATE cxx_std_20)
    target_compile_definitions(pegium-example-statemachine-test PRIVATE
        PEGIUM_EXAMPLE_STATEMACHINE_CLI_PATH="$<TARGET_FILE:pegium-example-statemachine-cli>"
    )
    target_link_libraries(pegium-example-statemachine-test PRIVATE
        pegium_example_statemachine_core
        pegium_example_statemachine_lsp
        pegium_example_statemachine_cli
        pegium_test_support_examples
        GTest::gtest_main
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
    )
    add_dependencies(pegium-example-statemachine-test
        pegium-example-statemachine-cli
    )
    pegium_register_gtest_target(pegium-example-statemachine-test LABELS
        example
        statemachine
    )
    add_test(
        NAME PegiumRecoveryCorpusStatemachine
        COMMAND $<TARGET_FILE:pegium-example-statemachine-test>
            --gtest_filter=*RecoverySample*:*RecoveryProbe*
    )
    set_tests_properties(PegiumRecoveryCorpusStatemachine PROPERTIES
        LABELS "recovery-corpus"
    )
endif()
