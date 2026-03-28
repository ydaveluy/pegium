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

pegium_register_gtest_target(PegiumWorkspaceFuzzTest LABELS fuzz)

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
