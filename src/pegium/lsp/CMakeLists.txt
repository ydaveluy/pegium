file(GLOB_RECURSE PEGIUM_LSP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set(PEGIUM_LSP_LIBRARY_ARGS ${PEGIUM_LSP_SOURCES})
if(NOT PEGIUM_MASTER_PROJECT)
    list(PREPEND PEGIUM_LSP_LIBRARY_ARGS EXCLUDE_FROM_ALL)
endif()

add_library(PegiumLsp ${PEGIUM_LSP_LIBRARY_ARGS})
add_library(pegium::lsp ALIAS PegiumLsp)

target_include_directories(PegiumLsp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_compile_features(PegiumLsp PUBLIC cxx_std_20)

target_link_libraries(PegiumLsp
    PUBLIC
        pegium::core
        pegium::converters
        lsp::lsp
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
