file(GLOB_RECURSE PEGIUM_CORE_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    converter/*.cpp
    documentation/*.cpp
    execution/*.cpp
    parser/*.cpp
    references/*.cpp
    services/*.cpp
    syntax-tree/*.cpp
    utils/*.cpp
    validation/*.cpp
    workspace/*.cpp
)

add_executable(PegiumCoreUnitTest ${PEGIUM_CORE_UNIT_TEST_SOURCES})

target_link_libraries(PegiumCoreUnitTest PRIVATE
    pegium::core
    pegium::converters
    pegium_test_support_core
    GTest::gtest_main
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

pegium_register_gtest_target(PegiumCoreUnitTest LABELS core)

add_test(
    NAME PegiumRecoveryCore
    COMMAND $<TARGET_FILE:PegiumCoreUnitTest>
        --gtest_filter=Recovery*:*GroupTest.*:*RepetitionTest.*:*TerminalRuleTest.*:*LiteralFuzzyMatcherTest.*
)
set_tests_properties(PegiumRecoveryCore PROPERTIES
    LABELS "recovery-core"
)

if(PEGIUM_ENABLE_ASAN OR PEGIUM_ENABLE_UBSAN)
    add_test(
        NAME PegiumCoreParserFocusedSanitizer
        COMMAND $<TARGET_FILE:PegiumCoreUnitTest>
            --gtest_filter=AssignmentTest.*:DataTypeRuleTest.*:DocumentTest.*:InfixRuleTest.*:ParserRuleTest.*:ParserTest.*:Recovery*:*TerminalRuleTest.*:DefaultDocumentBuilderTest.Parse*:DefaultDocumentFactoryTest.*
    )
    set_tests_properties(PegiumCoreParserFocusedSanitizer PROPERTIES
        LABELS sanitizer
    )
endif()
