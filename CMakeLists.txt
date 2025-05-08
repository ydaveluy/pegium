cmake_minimum_required(VERSION 3.14)

project(pegium LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    lsp-framework
    GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
    GIT_TAG        master
)

#FetchContent_MakeAvailable(lsp-framework)

#set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_CLANG_TIDY clang-tidy)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules-ts")

option(PEGIUM_ENABLE_CODECOVERAGE   "Enable code coverage testing support"            OFF)


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


file(GLOB_RECURSE SRC CONFIGURE_DEPENDS src/pegium/**.cpp)

add_library(pegium ${SRC})
target_include_directories(pegium PUBLIC src)
target_compile_features(pegium PUBLIC cxx_std_20)
include(CTest)


if(BUILD_TESTING)

    include(FetchContent)
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

    file(GLOB_RECURSE SRC-TEST CONFIGURE_DEPENDS 
    #tests/pegium/test.cpp 
    #tests/pegium/pegium.cpp
    #tests/pegium/assert.cpp
    #tests/pegium/xsmp2.cpp
   tests/pegium/xsmp.cpp
   tests/pegium/json.cpp
   tests/pegium/parser/*.cpp
    #tests/pegium/grammar.cpp 
    #tests/pegium/recovery.cpp
    )# 
    add_executable (PegiumTest ${SRC-TEST})

    target_include_directories(PegiumTest PRIVATE tests)
    target_link_libraries(PegiumTest PUBLIC
                                      pegium
                                      GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(PegiumTest)


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
                --delete --sort-uncovered --print-summary --calls --decisions ${GCOVR_OPTIONS}
                ${CMAKE_BINARY_DIR}
            COMMAND echo Generated report: ${CMAKE_BINARY_DIR}/report/index.html
            DEPENDS PegiumTest
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif(PEGIUM_ENABLE_CODECOVERAGE)
endif()

