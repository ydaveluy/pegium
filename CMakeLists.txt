cmake_minimum_required(VERSION 3.14)

project(pegium LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

set(LSP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LSP_INSTALL OFF CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED_LSP_FRAMEWORK ON CACHE BOOL "" FORCE)
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_LSP_FRAMEWORK
   AND EXISTS "${CMAKE_BINARY_DIR}/_deps/lsp-framework-src/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_LSP_FRAMEWORK
        "${CMAKE_BINARY_DIR}/_deps/lsp-framework-src"
        CACHE PATH "Override local source path for lsp_framework" FORCE)
endif()

FetchContent_Declare(
    lsp_framework
    GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
    GIT_TAG        ab8e884c01567debb319834ce90910f64616a12b
)
FetchContent_MakeAvailable(lsp_framework)

if(TARGET lsp AND NOT TARGET lsp::lsp)
    add_library(lsp::lsp ALIAS lsp)
endif()

#set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_CLANG_TIDY clang-tidy)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules-ts")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

option(PEGIUM_PROFILE_BUILD "Enable profiling-friendly flags (no LTO, visible symbols, frame pointers)" OFF)
option(PEGIUM_ENABLE_NATIVE_OPTIMIZATIONS "Enable -march=native on Release builds" OFF)
option(PEGIUM_ENABLE_FAST_MATH "Enable -ffast-math on optimized builds" OFF)
option(PEGIUM_ENABLE_ASAN "Enable AddressSanitizer for supported toolchains" OFF)
option(PEGIUM_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for supported toolchains" OFF)
option(PEGIUM_BUILD_FUZZ_TARGETS "Build standalone parser fuzz targets" OFF)

set(PEGIUM_SANITIZER_FLAGS)
if(PEGIUM_ENABLE_ASAN)
    list(APPEND PEGIUM_SANITIZER_FLAGS -fsanitize=address)
endif()
if(PEGIUM_ENABLE_UBSAN)
    list(APPEND PEGIUM_SANITIZER_FLAGS -fsanitize=undefined)
endif()
set(PEGIUM_SANITIZERS_ENABLED OFF)

if(PEGIUM_SANITIZER_FLAGS)
    set(PEGIUM_SANITIZERS_ENABLED ON)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "PEGIUM_ENABLE_ASAN/PEGIUM_ENABLE_UBSAN require GCC or Clang.")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
    add_compile_options(${PEGIUM_SANITIZER_FLAGS} -fno-omit-frame-pointer)
    add_link_options(${PEGIUM_SANITIZER_FLAGS})
endif()

if(PEGIUM_PROFILE_BUILD)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
    add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fno-lto>)
    add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>)
    add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fno-omit-frame-pointer>)
    add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fno-optimize-sibling-calls>)
    add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fno-inline>)
elseif(NOT PEGIUM_SANITIZERS_ENABLED)
    # Optimization flags for better performance
    add_compile_options($<$<CONFIG:Release>:-O3>)
    add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-flto>)  # Link-time optimization
    add_compile_options(-fvisibility=hidden)  # Hide symbols to reduce binary size and improve locality
    add_compile_options(-fvisibility-inlines-hidden)
    if(PEGIUM_ENABLE_NATIVE_OPTIMIZATIONS)
        add_compile_options($<$<CONFIG:Release>:-march=native>)
    endif()
    if(PEGIUM_ENABLE_FAST_MATH)
        add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-ffast-math>)  # Allow aggressive math optimizations
    endif()
endif()

option(PEGIUM_ENABLE_CODECOVERAGE   "Enable code coverage testing support"            OFF)
option(PEGIUM_ENABLE_RECOVERY_TRACE "Enable verbose recovery traces"                   OFF)

if(PEGIUM_ENABLE_RECOVERY_TRACE)
    add_compile_definitions(PEGIUM_ENABLE_RECOVERY_TRACE)
endif()


if(PEGIUM_ENABLE_CODECOVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang" )
        add_compile_options(--coverage)
        add_link_options(--coverage)
        set(GCOVR_OPTIONS --gcov-executable "llvm-cov gcov")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
        set(GCOVR_OPTIONS)
    else()
        message( ERROR "Code coverage is not supported for ${CMAKE_CXX_COMPILER_ID}" )
    endif()
endif(PEGIUM_ENABLE_CODECOVERAGE)


add_subdirectory(src/pegium)
option(PEGIUM_BUILD_EXAMPLES "Build pegium examples" ON)
if(PEGIUM_BUILD_EXAMPLES)
    add_subdirectory(examples/arithmetics)
    add_subdirectory(examples/domainmodel)
    add_subdirectory(examples/requirements)
    add_subdirectory(examples/statemachine)
endif()

include(CTest)


if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "")
    set(INSTALL_GTEST OFF CACHE BOOL "")
    FetchContent_MakeAvailable(googletest)

    file(GLOB_RECURSE PEGIUM_CORE_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        tests/pegium/converter/*.cpp
        tests/pegium/execution/*.cpp
        tests/pegium/documentation/*.cpp
        tests/pegium/parser/*.cpp
        tests/pegium/references/*.cpp
        tests/pegium/services/*.cpp
        tests/pegium/syntax-tree/*.cpp
        tests/pegium/utils/*.cpp
        tests/pegium/validation/*.cpp
        tests/pegium/workspace/*.cpp
    )

    file(GLOB_RECURSE PEGIUM_LSP_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        tests/pegium/lsp/*.cpp
    )

    file(GLOB_RECURSE PEGIUM_INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
        tests/pegium/integration/*.cpp
    )

    if(PEGIUM_BUILD_FUZZ_TARGETS)
        file(GLOB_RECURSE PEGIUM_FUZZ_SOURCES CONFIGURE_DEPENDS
            tests/fuzz/*.cpp
        )
    endif()

    if(PEGIUM_BUILD_EXAMPLES)
        file(GLOB_RECURSE PEGIUM_BENCH_SOURCES CONFIGURE_DEPENDS
            tests/bench/*.cpp
        )
    endif()

    if(PEGIUM_BUILD_EXAMPLES)
        file(GLOB_RECURSE PEGIUM_EXAMPLE_TEST_SOURCES CONFIGURE_DEPENDS
            examples/*/tests/*.cpp
        )
    endif()

    file(GLOB_RECURSE PEGIUM_LEGACY_LANGUAGE_TEST_SOURCES CONFIGURE_DEPENDS
        tests-legacy/pegium-legacy-20260307/languages/*.cpp
    )

    add_executable(PegiumCoreUnitTest ${PEGIUM_CORE_UNIT_TEST_SOURCES})
    add_executable(PegiumLspUnitTest ${PEGIUM_LSP_UNIT_TEST_SOURCES})
    add_executable(PegiumIntegrationTest ${PEGIUM_INTEGRATION_TEST_SOURCES})
    add_executable(PegiumLegacyLanguagesTest ${PEGIUM_LEGACY_LANGUAGE_TEST_SOURCES})
    if(PEGIUM_BUILD_EXAMPLES)
        add_executable(PegiumBench ${PEGIUM_BENCH_SOURCES})
        add_executable(PegiumExamplesTest ${PEGIUM_EXAMPLE_TEST_SOURCES})
    endif()
    if(PEGIUM_BUILD_FUZZ_TARGETS)
        add_executable(PegiumParserFuzzTarget ${PEGIUM_FUZZ_SOURCES})
    endif()

    target_include_directories(PegiumCoreUnitTest PRIVATE tests)
    target_include_directories(PegiumLspUnitTest PRIVATE tests)
    target_include_directories(PegiumIntegrationTest PRIVATE tests)
    target_include_directories(PegiumLegacyLanguagesTest PRIVATE
        tests-legacy/include
        tests-legacy/pegium-legacy-20260307
    )
    if(PEGIUM_BUILD_EXAMPLES)
        target_include_directories(PegiumBench PRIVATE tests)
        target_include_directories(PegiumExamplesTest PRIVATE tests)
    endif()
    if(PEGIUM_BUILD_FUZZ_TARGETS)
        target_include_directories(PegiumParserFuzzTarget PRIVATE tests)
    endif()

    target_link_libraries(PegiumCoreUnitTest PUBLIC
        PegiumCore
        GTest::gtest_main
    )
    target_link_libraries(PegiumLspUnitTest PUBLIC
        pegium
        GTest::gtest_main
    )
    target_link_libraries(PegiumIntegrationTest PUBLIC
        pegium
        GTest::gtest_main
    )
    target_link_libraries(PegiumLegacyLanguagesTest PUBLIC
        PegiumCore
        GTest::gtest_main
    )
    add_custom_target(PegiumParserPerfCheck
        COMMAND ${CMAKE_COMMAND} -E env
            PEGIUM_BENCH_ITERATIONS=1
            PEGIUM_BENCH_WARMUP=0
            PEGIUM_BENCH_SAMPLES=1
            bash ${CMAKE_SOURCE_DIR}/tools/check_parser_perf.sh
                $<TARGET_FILE:PegiumLegacyLanguagesTest>
        DEPENDS PegiumLegacyLanguagesTest
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
        COMMENT "Running parser callgrind perf baselines"
    )
    if(PEGIUM_BUILD_FUZZ_TARGETS)
        target_link_libraries(PegiumParserFuzzTarget PUBLIC PegiumCore)
    endif()
    if(PEGIUM_BUILD_EXAMPLES)
        target_link_libraries(PegiumIntegrationTest PUBLIC
            pegium_example_arithmetics
            pegium_example_domainmodel
            pegium_example_requirements
            pegium_example_statemachine
        )
    endif()
    if(PEGIUM_BUILD_EXAMPLES)
        target_link_libraries(PegiumBench PUBLIC
            pegium
            pegium_example_arithmetics
            pegium_example_domainmodel
            pegium_example_requirements
            pegium_example_statemachine
        )
    endif()
    if(PEGIUM_BUILD_EXAMPLES)
        target_link_libraries(PegiumExamplesTest PUBLIC
            pegium
            pegium_example_arithmetics
            pegium_example_domainmodel
            pegium_example_requirements
            pegium_example_statemachine
            GTest::gtest_main
        )
    endif()

    include(GoogleTest)
    gtest_discover_tests(PegiumCoreUnitTest DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(PegiumLspUnitTest DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(PegiumIntegrationTest DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(PegiumLegacyLanguagesTest DISCOVERY_MODE PRE_TEST)
    if(PEGIUM_BUILD_EXAMPLES)
        gtest_discover_tests(PegiumExamplesTest DISCOVERY_MODE PRE_TEST)
    endif()
    if(PEGIUM_BUILD_FUZZ_TARGETS)
        add_test(
            NAME PegiumParserFuzzSmoke
            COMMAND PegiumParserFuzzTarget --corpus
                    ${CMAKE_SOURCE_DIR}/tests/fuzz/corpus
        )
        set_tests_properties(PegiumParserFuzzSmoke PROPERTIES
            LABELS "fuzz;smoke"
            TIMEOUT 60
        )
    endif()


    if(PEGIUM_ENABLE_CODECOVERAGE)
        include(CTest)
        find_program(GCOVR_PATH gcovr REQUIRED)
        add_custom_target(coverage
            COMMAND rm -rf $$\(find . -name \"*.gcda\"\)
            # run tests
            COMMAND ${CMAKE_CTEST_COMMAND} -T Test --output-on-failure
            # generate report
            COMMAND mkdir -p ${CMAKE_BINARY_DIR}/report
            COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND ${GCOVR_PATH} --root . --html-nested ${CMAKE_BINARY_DIR}/report/index.html --html-title \"pegium Code Coverage Report\"
                -f \"src/\"
                --delete --sort uncovered-number --print-summary --calls --decisions
                --gcov-ignore-parse-errors negative_hits.warn_once_per_file
                ${GCOVR_OPTIONS}
                ${CMAKE_BINARY_DIR}
            COMMAND echo Generated report: ${CMAKE_BINARY_DIR}/report/index.html
            DEPENDS PegiumCoreUnitTest PegiumLspUnitTest PegiumIntegrationTest PegiumLegacyLanguagesTest
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        if(PEGIUM_BUILD_EXAMPLES)
            add_dependencies(coverage PegiumExamplesTest)
        endif()
    endif(PEGIUM_ENABLE_CODECOVERAGE)
endif()
