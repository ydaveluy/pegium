file(GLOB_RECURSE PEGIUM_CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(PegiumCli STATIC ${PEGIUM_CLI_SOURCES})

target_include_directories(PegiumCli
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(PegiumCli PUBLIC cxx_std_20)

target_compile_options(PegiumCli PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-visibility-inlines-hidden>
)

target_link_libraries(PegiumCli PUBLIC Pegium)
