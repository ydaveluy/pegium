set(PEGIUM_FUZZ_TARGET_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AdversarialLanguage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StressLanguage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkspaceFuzzHarness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkspaceFuzzTest.cpp
)

add_executable(PegiumWorkspaceFuzzTest ${PEGIUM_FUZZ_TARGET_SOURCES})
target_link_libraries(PegiumWorkspaceFuzzTest PRIVATE
    pegium::lsp
    pegium::test_support_lsp
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
link_fuzztest(PegiumWorkspaceFuzzTest)

# Register only the deterministic `PegiumWorkspaceRegressionTest.*` fixtures as
# ctest entries (they pin the curated adversarial cases and run in <1s total).
# The `FUZZ_TEST` entries are coverage-guided targets whose value comes from the
# instrumented `--fuzz=...` runs under `-DFUZZTEST_FUZZING_MODE=ON`. Running them
# in a normal build's unit-test mode just replays their whole adversarial seed
# corpus through the full workspace pipeline with no coverage feedback — e.g.
# `AdversarialLanguageSingleDocumentPipeline` alone takes ~130s for the seed
# replay (which `FUZZTEST_MAX_FUZZING_TIME` cannot bound). That cost dominated
# the default `ctest` for little value, so the fuzzers are left for the manual /
# fuzzing-mode `--fuzz=...` path in both configurations.
pegium_register_gtest_target(PegiumWorkspaceFuzzTest
    LABELS fuzz
    TEST_FILTER "PegiumWorkspaceRegressionTest.*"
)

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
        pegium::test_support_examples
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
    )
    link_fuzztest(PegiumExampleKeywordFuzzTest)

    pegium_register_gtest_target(PegiumExampleKeywordFuzzTest LABELS fuzz)
endif()
