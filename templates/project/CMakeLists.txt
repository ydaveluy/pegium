cmake_minimum_required(VERSION 3.14)
project(@PEGIUM_NEW_LANGUAGE_ID@ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    pegium
    GIT_REPOSITORY https://github.com/ydaveluy/pegium.git
    GIT_TAG @PEGIUM_NEW_PEGIUM_TAG@
)
FetchContent_MakeAvailable(pegium)

add_library(@PEGIUM_NEW_LANGUAGE_ID@-core
    src/@PEGIUM_NEW_LANGUAGE_ID@/core/Module.cpp
)
target_include_directories(@PEGIUM_NEW_LANGUAGE_ID@-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(@PEGIUM_NEW_LANGUAGE_ID@-core PUBLIC pegium::core)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/@PEGIUM_NEW_LANGUAGE_ID@/cli/main.cpp")
  add_executable(@PEGIUM_NEW_LANGUAGE_ID@-cli src/@PEGIUM_NEW_LANGUAGE_ID@/cli/main.cpp)
  target_link_libraries(@PEGIUM_NEW_LANGUAGE_ID@-cli PRIVATE @PEGIUM_NEW_LANGUAGE_ID@-core pegium::cli)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/@PEGIUM_NEW_LANGUAGE_ID@/lsp/Module.cpp")
  add_library(@PEGIUM_NEW_LANGUAGE_ID@-lsp-lib src/@PEGIUM_NEW_LANGUAGE_ID@/lsp/Module.cpp)
  target_include_directories(@PEGIUM_NEW_LANGUAGE_ID@-lsp-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(@PEGIUM_NEW_LANGUAGE_ID@-lsp-lib PUBLIC @PEGIUM_NEW_LANGUAGE_ID@-core pegium::lsp)

  add_executable(@PEGIUM_NEW_LANGUAGE_ID@-lsp src/@PEGIUM_NEW_LANGUAGE_ID@/lsp/main.cpp)
  target_link_libraries(@PEGIUM_NEW_LANGUAGE_ID@-lsp PRIVATE @PEGIUM_NEW_LANGUAGE_ID@-lsp-lib)
endif()

enable_testing()
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(@PEGIUM_NEW_LANGUAGE_ID@-test test/parsing_test.cpp)
target_link_libraries(@PEGIUM_NEW_LANGUAGE_ID@-test PRIVATE @PEGIUM_NEW_LANGUAGE_ID@-core pegium::cli GTest::gtest_main)
target_compile_definitions(@PEGIUM_NEW_LANGUAGE_ID@-test PRIVATE
    PEGIUM_NEW_SAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/example/hello@PEGIUM_NEW_EXT@")
add_test(NAME @PEGIUM_NEW_LANGUAGE_ID@-parse COMMAND @PEGIUM_NEW_LANGUAGE_ID@-test)
