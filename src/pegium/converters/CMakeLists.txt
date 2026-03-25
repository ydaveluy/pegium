file(GLOB_RECURSE PEGIUM_CONVERTER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set(PEGIUM_CONVERTERS_LIBRARY_ARGS ${PEGIUM_CONVERTER_SOURCES})
if(NOT PEGIUM_MASTER_PROJECT)
    list(PREPEND PEGIUM_CONVERTERS_LIBRARY_ARGS EXCLUDE_FROM_ALL)
endif()

add_library(PegiumConverters ${PEGIUM_CONVERTERS_LIBRARY_ARGS})
add_library(pegium::converters ALIAS PegiumConverters)

target_include_directories(PegiumConverters
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_compile_features(PegiumConverters PUBLIC cxx_std_20)

target_link_libraries(PegiumConverters
    PUBLIC
        pegium::core
    PRIVATE
        ${PEGIUM_INTERNAL_OPTION_TARGETS}
)
