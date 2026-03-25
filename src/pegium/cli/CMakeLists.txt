file(GLOB_RECURSE PEGIUM_CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set(PEGIUM_CLI_LIBRARY_ARGS ${PEGIUM_CLI_SOURCES})
if(NOT PEGIUM_MASTER_PROJECT)
    list(PREPEND PEGIUM_CLI_LIBRARY_ARGS EXCLUDE_FROM_ALL)
endif()

add_library(PegiumCli ${PEGIUM_CLI_LIBRARY_ARGS})
add_library(pegium::cli ALIAS PegiumCli)

target_include_directories(PegiumCli
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_compile_features(PegiumCli PUBLIC cxx_std_20)

target_link_libraries(PegiumCli
    PUBLIC
        pegium::core
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
