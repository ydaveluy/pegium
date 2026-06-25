# pegium::testing — reusable GoogleTest-based test harness for languages built
# on pegium. Public headers stay free of pegium internals; the LSP feature
# dispatch is used only inside the compiled translation unit.

if(NOT TARGET GTest::gtest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
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

file(GLOB_RECURSE PEGIUM_TESTING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set(PEGIUM_TESTING_LIBRARY_ARGS ${PEGIUM_TESTING_SOURCES})
if(NOT PEGIUM_MASTER_PROJECT)
    list(PREPEND PEGIUM_TESTING_LIBRARY_ARGS EXCLUDE_FROM_ALL)
endif()

add_library(PegiumTesting ${PEGIUM_TESTING_LIBRARY_ARGS})
add_library(pegium::testing ALIAS PegiumTesting)

target_include_directories(PegiumTesting
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_compile_features(PegiumTesting PUBLIC cxx_std_20)

target_link_libraries(PegiumTesting
    PUBLIC
        pegium::lsp
        GTest::gtest
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
