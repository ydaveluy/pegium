file(GLOB_RECURSE PEGIUM_INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
    *.cpp
)

add_executable(PegiumIntegrationTest ${PEGIUM_INTEGRATION_TEST_SOURCES})

target_link_libraries(PegiumIntegrationTest PRIVATE
    pegium::lsp
    pegium_test_support_lsp
    GTest::gtest_main
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)

pegium_register_gtest_target(PegiumIntegrationTest LABELS integration)
