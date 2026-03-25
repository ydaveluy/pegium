get_property(PEGIUM_EXAMPLE_LANGUAGE_TARGETS GLOBAL
    PROPERTY PEGIUM_EXAMPLE_LANGUAGE_TARGETS)

if(NOT PEGIUM_EXAMPLE_LANGUAGE_TARGETS)
    message(FATAL_ERROR
        "PEGIUM_BUILD_EXAMPLES is ON but no example language targets were registered.")
endif()

file(GLOB_RECURSE PEGIUM_BENCH_SOURCES CONFIGURE_DEPENDS
    *.cpp
)

add_executable(PegiumBench ${PEGIUM_BENCH_SOURCES})
target_include_directories(PegiumBench PRIVATE ${PEGIUM_TEST_ROOT_DIR})
target_link_libraries(PegiumBench PRIVATE
    pegium::lsp
    ${PEGIUM_EXAMPLE_LANGUAGE_TARGETS}
    ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
