add_subdirectory(pegium)

if(PEGIUM_BUILD_EXAMPLES)
    add_subdirectory(bench)
endif()

if(PEGIUM_BUILD_FUZZ_TARGETS)
    add_subdirectory(fuzz)
endif()

add_subdirectory(cmake)
