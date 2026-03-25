include(FetchContent)

if(NOT TARGET lsp::lsp)
    if(TARGET lsp)
        add_library(lsp::lsp ALIAS lsp)
    else()
        if(NOT DEFINED LSP_BUILD_EXAMPLES)
            set(LSP_BUILD_EXAMPLES OFF CACHE BOOL "")
        endif()
        if(NOT DEFINED LSP_INSTALL)
            set(LSP_INSTALL OFF CACHE BOOL "")
        endif()

        FetchContent_Declare(
            lsp_framework
            GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
            GIT_TAG        1.3.1
        )
        FetchContent_MakeAvailable(lsp_framework)

        if(TARGET lsp AND NOT TARGET lsp::lsp)
            add_library(lsp::lsp ALIAS lsp)
        endif()
    endif()
endif()

add_subdirectory(core)
add_subdirectory(converters)
add_subdirectory(lsp)
add_subdirectory(cli)
