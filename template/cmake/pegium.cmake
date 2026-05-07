# Fetches the Pegium framework via FetchContent.
#
# Bump `GIT_TAG` to upgrade the language to a newer Pegium release.
# Default = `main`; pin to a release tag for reproducible builds.

include(FetchContent)

FetchContent_Declare(
    pegium
    GIT_REPOSITORY https://github.com/ydaveluy/pegium.git
    GIT_TAG        main
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PEGIUM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(pegium)
