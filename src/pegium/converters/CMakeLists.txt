file(GLOB_RECURSE PEGIUM_CONVERTER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(PegiumConverters STATIC ${PEGIUM_CONVERTER_SOURCES})

target_include_directories(PegiumConverters
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(PegiumConverters PUBLIC cxx_std_20)

target_compile_options(PegiumConverters PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-visibility-inlines-hidden>
)

target_link_libraries(PegiumConverters PUBLIC PegiumCore)
