file(GLOB_RECURSE PEGIUM_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

set(PEGIUM_CORE_LIBRARY_ARGS ${PEGIUM_CORE_SOURCES})
if(NOT PEGIUM_MASTER_PROJECT)
    list(PREPEND PEGIUM_CORE_LIBRARY_ARGS EXCLUDE_FROM_ALL)
endif()

add_library(PegiumCore ${PEGIUM_CORE_LIBRARY_ARGS})
add_library(pegium::core ALIAS PegiumCore)

target_include_directories(PegiumCore
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_compile_features(PegiumCore PUBLIC cxx_std_20)
target_link_libraries(PegiumCore PRIVATE ${PEGIUM_INTERNAL_OPTION_TARGETS})
