cmake_minimum_required(VERSION 3.14)

project(pegium LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PEGIUM_MASTER_PROJECT ON)
else()
    set(PEGIUM_MASTER_PROJECT OFF)
endif()

option(PEGIUM_PROFILE_BUILD "Enable profiling-friendly flags (no LTO, visible symbols, frame pointers)" OFF)
option(PEGIUM_ENABLE_ASAN "Enable AddressSanitizer for supported toolchains" OFF)
option(PEGIUM_ENABLE_TSAN "Enable ThreadSanitizer for supported toolchains" OFF)
option(PEGIUM_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for supported toolchains" OFF)
option(PEGIUM_BUILD_FUZZ_TARGETS "Build FuzzTest-based Pegium fuzz targets" OFF)
# The exhaustive language fuzz corpus sweeps (the deterministic *KeywordFuzzTest
# / *SemicolonColonFuzzTest mutation sweeps) are cheap in optimized builds
# (~6s total) but slow under sanitizers or unoptimized Debug builds. Default
# them ON only where they are cheap so a plain `cmake` build still exercises
# recovery on every mutation; override with -DPEGIUM_ENABLE_FUZZ_CORPUS=ON/OFF.
if(PEGIUM_ENABLE_ASAN OR PEGIUM_ENABLE_TSAN OR PEGIUM_ENABLE_UBSAN
   OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PEGIUM_FUZZ_CORPUS_DEFAULT OFF)
else()
    set(PEGIUM_FUZZ_CORPUS_DEFAULT ON)
endif()
option(PEGIUM_ENABLE_FUZZ_CORPUS
    "Register exhaustive language-specific fuzz corpus tests (default ON for optimized non-sanitizer builds; cheap there, slow under Debug/ASan)"
    ${PEGIUM_FUZZ_CORPUS_DEFAULT})
option(PEGIUM_BUILD_EXAMPLES "Build pegium examples" ${PEGIUM_MASTER_PROJECT})

if(NOT PEGIUM_MASTER_PROJECT AND NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF CACHE BOOL "Build the test suite")
endif()
include(CTest)

if(BUILD_TESTING)
    include(FetchContent)
    include(GoogleTest)

    set(PEGIUM_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
    set(PEGIUM_TEST_ROOT_DIR "${PROJECT_SOURCE_DIR}/tests")

    if(NOT TARGET GTest::gtest_main)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.17.0
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        if(NOT DEFINED gtest_force_shared_crt)
            set(gtest_force_shared_crt ON CACHE BOOL "")
        endif()
        if(NOT DEFINED BUILD_GMOCK)
            set(BUILD_GMOCK OFF CACHE BOOL "")
        endif()
        if(NOT DEFINED INSTALL_GTEST)
            set(INSTALL_GTEST OFF CACHE BOOL "")
        endif()
        FetchContent_MakeAvailable(googletest)
    endif()

    add_library(pegium_test_support_core INTERFACE)
    target_include_directories(pegium_test_support_core INTERFACE
        $<BUILD_INTERFACE:${PEGIUM_TEST_ROOT_DIR}>
    )

    add_library(pegium_test_support_lsp INTERFACE)
    target_link_libraries(pegium_test_support_lsp INTERFACE
        pegium_test_support_core
    )

    add_library(pegium_test_support_examples INTERFACE)
    target_link_libraries(pegium_test_support_examples INTERFACE
        pegium_test_support_lsp
    )

    function(pegium_register_gtest_target target)
        set(options)
        set(oneValueArgs TEST_FILTER)
        set(multiValueArgs LABELS)
        cmake_parse_arguments(PEGIUM_GTEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        set(discover_args DISCOVERY_MODE PRE_TEST)
        if(PEGIUM_GTEST_TEST_FILTER)
            list(APPEND discover_args TEST_FILTER "${PEGIUM_GTEST_TEST_FILTER}")
        endif()
        if(PEGIUM_GTEST_LABELS)
            list(APPEND discover_args PROPERTIES LABELS "${PEGIUM_GTEST_LABELS}")
        endif()
        gtest_discover_tests(${target} ${discover_args})

        set_property(GLOBAL APPEND PROPERTY PEGIUM_GTEST_TARGETS ${target})
    endfunction()
endif()

add_library(pegium_internal_build_options INTERFACE)
add_library(pegium_internal_coverage_options INTERFACE)
set(PEGIUM_INTERNAL_OPTION_TARGETS
    pegium_internal_build_options
    pegium_internal_coverage_options
)

set(PEGIUM_SANITIZER_FLAGS)
if(PEGIUM_ENABLE_ASAN AND PEGIUM_ENABLE_TSAN)
    message(FATAL_ERROR
        "PEGIUM_ENABLE_ASAN and PEGIUM_ENABLE_TSAN cannot be enabled together.")
endif()
if(PEGIUM_ENABLE_ASAN)
    list(APPEND PEGIUM_SANITIZER_FLAGS -fsanitize=address)
endif()
if(PEGIUM_ENABLE_TSAN)
    list(APPEND PEGIUM_SANITIZER_FLAGS -fsanitize=thread)
endif()
if(PEGIUM_ENABLE_UBSAN)
    list(APPEND PEGIUM_SANITIZER_FLAGS -fsanitize=undefined)
endif()
if(PEGIUM_SANITIZER_FLAGS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "PEGIUM_ENABLE_ASAN/PEGIUM_ENABLE_TSAN/PEGIUM_ENABLE_UBSAN require GCC or Clang.")
    endif()
    target_compile_options(pegium_internal_build_options INTERFACE
        ${PEGIUM_SANITIZER_FLAGS}
        -fno-omit-frame-pointer
    )
    target_link_options(pegium_internal_build_options INTERFACE
        ${PEGIUM_SANITIZER_FLAGS}
    )
endif()

if(PEGIUM_ENABLE_ASAN)
    target_compile_definitions(pegium_internal_build_options INTERFACE
        ADDRESS_SANITIZER
    )
endif()

if(PEGIUM_ENABLE_TSAN)
    target_compile_definitions(pegium_internal_build_options INTERFACE
        THREAD_SANITIZER
    )
endif()

if(PEGIUM_PROFILE_BUILD)
    target_compile_options(pegium_internal_build_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-omit-frame-pointer>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-optimize-sibling-calls>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-inline>
    )
endif()

option(PEGIUM_ENABLE_CODECOVERAGE   "Enable code coverage testing support"            OFF)
option(PEGIUM_ENABLE_RECOVERY_TRACE "Enable verbose recovery traces"                   OFF)
option(PEGIUM_ENABLE_STEP_TRACE     "Enable step counters for recovery instrumentation" OFF)

if(PEGIUM_ENABLE_RECOVERY_TRACE)
    target_compile_definitions(pegium_internal_build_options INTERFACE
        PEGIUM_ENABLE_RECOVERY_TRACE
    )
endif()

if(PEGIUM_ENABLE_STEP_TRACE)
    target_compile_definitions(pegium_internal_build_options INTERFACE
        PEGIUM_ENABLE_STEP_TRACE
    )
endif()

if(PEGIUM_ENABLE_CODECOVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang")
        target_compile_options(pegium_internal_coverage_options INTERFACE
            --coverage
        )
        target_link_options(pegium_internal_coverage_options INTERFACE
            --coverage
        )
        set(GCOVR_OPTIONS --gcov-executable "llvm-cov gcov")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(pegium_internal_coverage_options INTERFACE
            --coverage
        )
        target_link_options(pegium_internal_coverage_options INTERFACE
            --coverage
        )
        set(GCOVR_OPTIONS)
    else()
        message(FATAL_ERROR
            "Code coverage is not supported for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

if(PEGIUM_BUILD_FUZZ_TARGETS)
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR
            "PEGIUM_BUILD_FUZZ_TARGETS requires BUILD_TESTING=ON.")
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR
            "PEGIUM_BUILD_FUZZ_TARGETS is only supported on Linux.")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "PEGIUM_BUILD_FUZZ_TARGETS requires Clang.")
    endif()

    if(FUZZTEST_FUZZING_MODE)
        target_compile_definitions(pegium_internal_build_options INTERFACE
            ADDRESS_SANITIZER
            FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        )
        target_compile_options(pegium_internal_build_options INTERFACE
            -g
            -UNDEBUG
            -fsanitize=address
            -fsanitize-coverage=inline-8bit-counters
            -fsanitize-coverage=trace-cmp
        )
        target_link_options(pegium_internal_build_options INTERFACE
            -fsanitize=address
        )
    endif()

    if(NOT COMMAND link_fuzztest)
        FetchContent_Declare(
            fuzztest
            GIT_REPOSITORY https://github.com/google/fuzztest.git
            GIT_TAG        2026-02-19
        )
        if(NOT DEFINED FUZZTEST_BUILD_TESTING)
            set(FUZZTEST_BUILD_TESTING OFF CACHE BOOL "")
        endif()
        FetchContent_MakeAvailable(fuzztest)
    endif()
endif()

add_subdirectory(src/pegium)
if(PEGIUM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
