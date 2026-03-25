file(GLOB_RECURSE PEGIUM_LSP_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    *.cpp
)

add_executable(PegiumLspUnitTest ${PEGIUM_LSP_UNIT_TEST_SOURCES})

target_link_libraries(PegiumLspUnitTest PRIVATE
    pegium::lsp
    pegium::cli
    pegium_test_support_lsp
    GTest::gtest_main
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

pegium_register_gtest_target(PegiumLspUnitTest LABELS lsp)

if(PEGIUM_ENABLE_ASAN OR PEGIUM_ENABLE_UBSAN)
    add_test(
        NAME PegiumLspSemanticTokenSanitizer
        COMMAND $<TARGET_FILE:PegiumLspUnitTest>
            --gtest_filter=*SemanticToken*
    )
    set_tests_properties(PegiumLspSemanticTokenSanitizer PROPERTIES
        LABELS sanitizer
    )
endif()
