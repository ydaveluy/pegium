function(pegium_add_fetchcontent_smoke_test name)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DPEGIUM_SOURCE_DIR=${PEGIUM_PROJECT_SOURCE_DIR}
            -DPEGIUM_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fetchcontent-consumer
            -DPEGIUM_CONSUMER_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}
            -DPEGIUM_CMAKE_GENERATOR=${CMAKE_GENERATOR}
            -DPEGIUM_CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
            -DPEGIUM_CMAKE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
            -DPEGIUM_CMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DPEGIUM_CMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DPEGIUM_CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DPEGIUM_CMAKE_BUILD_TYPE=$<CONFIG>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/RunFetchContentSmoke.cmake
    )
    set_tests_properties(${name} PROPERTIES LABELS cmake)
endfunction()

pegium_add_fetchcontent_smoke_test(PegiumFetchContentSmoke)

if(PEGIUM_ENABLE_CODECOVERAGE)
    get_property(PEGIUM_GTEST_TARGETS GLOBAL PROPERTY PEGIUM_GTEST_TARGETS)
    find_program(GCOVR_PATH gcovr REQUIRED)

    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND}
            -DPEGIUM_COVERAGE_BINARY_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/ResetCoverageData.cmake
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/report
        COMMAND ${CMAKE_COMMAND} -E chdir ${PEGIUM_PROJECT_SOURCE_DIR}
            ${GCOVR_PATH} --root . --html-nested ${CMAKE_BINARY_DIR}/report/index.html --html-title \"pegium Code Coverage Report\"
            -f \"src/\"
            --delete --sort uncovered-number --print-summary --calls --decisions
            --gcov-ignore-parse-errors negative_hits.warn_once_per_file
            ${GCOVR_OPTIONS}
            ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo
            Generated report: ${CMAKE_BINARY_DIR}/report/index.html
        DEPENDS ${PEGIUM_GTEST_TARGETS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND_EXPAND_LISTS
    )
endif()
