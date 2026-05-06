set(PEGIUM_FUZZ_TARGET_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AdversarialLanguage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StressLanguage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkspaceFuzzHarness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkspaceFuzzTest.cpp
)

add_executable(PegiumWorkspaceFuzzTest ${PEGIUM_FUZZ_TARGET_SOURCES})
target_link_libraries(PegiumWorkspaceFuzzTest PRIVATE
    pegium::lsp
    pegium_test_support_lsp
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
link_fuzztest(PegiumWorkspaceFuzzTest)

# Coverage-guided fuzz mode (`-DFUZZTEST_FUZZING_MODE=ON`) registers each
# `FUZZ_TEST` as a ctest entry that, by default, runs every WithSeeds
# corpus entry against the instrumented binary. The instrumentation
# (ASan + sancov) makes parses 10-100x slower, so iterating thousands of
# adversarial seeds exceeds any reasonable test timeout. Restrict the
# ctest registration to the deterministic `PegiumWorkspaceRegressionTest.*`
# fixtures; the dynamic fuzz engine is still invoked manually with
# `--fuzz=...` for the FUZZ_TEST entries.
if(FUZZTEST_FUZZING_MODE)
    pegium_register_gtest_target(PegiumWorkspaceFuzzTest
        LABELS fuzz
        TEST_FILTER "PegiumWorkspaceRegressionTest.*"
    )
else()
    pegium_register_gtest_target(PegiumWorkspaceFuzzTest LABELS fuzz)
endif()

if(FUZZTEST_FUZZING_MODE)
    add_test(
        NAME PegiumWorkspaceGuidedFuzzSanity
        COMMAND $<TARGET_FILE:PegiumWorkspaceFuzzTest>
            --gtest_filter=PegiumWorkspaceRegressionTest.CoverageBracePairDeleteBurstRoundTripRemainsStable:PegiumWorkspaceRegressionTest.RecoveryDenseContextualSwapRoundTripRemainsStable:PegiumWorkspaceRegressionTest.TruncatedEofWindowSpliceRoundTripRemainsStable
    )
    set_tests_properties(PegiumWorkspaceGuidedFuzzSanity PROPERTIES
        LABELS fuzz-guided
        TIMEOUT 30
    )

endif()

# ----------------------------------------------------------------------
# PegiumExampleKeywordFuzzTest: coverage-guided keyword/symbol fuzz on
# the four example languages. Mirrors the static `*KeywordFuzzTest`
# GTests under each example tree but drives mutations through
# FuzzTest's coverage-guided engine.
# ----------------------------------------------------------------------
if(PEGIUM_BUILD_EXAMPLES)
    add_executable(PegiumExampleKeywordFuzzTest
        ${CMAKE_CURRENT_SOURCE_DIR}/ExampleKeywordFuzz.cpp
    )
    target_link_libraries(PegiumExampleKeywordFuzzTest PRIVATE
        pegium::core
        pegium::lsp
        pegium_example_arithmetics_core
        pegium_example_domainmodel_core
        pegium_example_statemachine_core
        pegium_example_requirements_core
        pegium_test_support_examples
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
    )
    link_fuzztest(PegiumExampleKeywordFuzzTest)

    pegium_register_gtest_target(PegiumExampleKeywordFuzzTest LABELS fuzz)
endif()
