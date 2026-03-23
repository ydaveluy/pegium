file(GLOB_RECURSE PEGIUM_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(PegiumCore STATIC ${PEGIUM_CORE_SOURCES})

target_include_directories(PegiumCore
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(PegiumCore PUBLIC cxx_std_20)

target_compile_options(PegiumCore PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fvisibility=default>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-visibility-inlines-hidden>
)
