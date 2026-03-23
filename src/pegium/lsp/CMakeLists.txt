file(GLOB_RECURSE PEGIUM_LSP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Pegium STATIC ${PEGIUM_LSP_SOURCES})

target_include_directories(Pegium
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(Pegium PUBLIC cxx_std_20)

target_compile_options(Pegium PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-visibility-inlines-hidden>
)

target_link_libraries(Pegium PUBLIC PegiumCore PegiumConverters lsp::lsp)
