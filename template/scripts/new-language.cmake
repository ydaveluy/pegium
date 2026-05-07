# Rename the template's default identifiers to user-supplied ones.
#
# Usage:
#   cmake -P scripts/new-language.cmake \
#       -DLANGUAGE_NAME=mylang \
#       [-DLANGUAGE_CLASS=MyLang] \
#       [-DFILE_EXT=.mylang]
#
# Replaces every occurrence of the template's default identifiers in file
# contents AND in path names. Operates in place; run on a fresh checkout.
#
# Token map (most-specific first to avoid double-renames):
#   .mydsl   -> .${LANGUAGE_NAME}      (file extension)
#   MyDsl    -> ${LANGUAGE_CLASS}      (PascalCase: class prefixes)
#   MYDSL    -> ${LANGUAGE_UPPER}      (UPPERCASE: macros, CMake vars)
#   mydsl    -> ${LANGUAGE_NAME}       (lowercase: namespaces, folders)

cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED LANGUAGE_NAME)
    message(FATAL_ERROR
        "LANGUAGE_NAME is required (e.g. -DLANGUAGE_NAME=mylang)")
endif()

# Sanity check: lowercase letters / digits / underscore only.
if(NOT LANGUAGE_NAME MATCHES "^[a-z][a-z0-9_]*$")
    message(FATAL_ERROR
        "LANGUAGE_NAME must match ^[a-z][a-z0-9_]*$ (got '${LANGUAGE_NAME}')")
endif()

# Derive the casings unless the caller overrode them.
if(NOT DEFINED LANGUAGE_CLASS)
    string(SUBSTRING "${LANGUAGE_NAME}" 0 1 _first)
    string(SUBSTRING "${LANGUAGE_NAME}" 1 -1 _rest)
    string(TOUPPER "${_first}" _first)
    set(LANGUAGE_CLASS "${_first}${_rest}")
endif()

if(NOT DEFINED FILE_EXT)
    set(FILE_EXT ".${LANGUAGE_NAME}")
endif()

string(TOUPPER "${LANGUAGE_NAME}" LANGUAGE_UPPER)

# Token pairs (most-specific first). Encoded as "from|to".
set(_token_pairs
    ".mydsl|${FILE_EXT}"
    "MyDsl|${LANGUAGE_CLASS}"
    "MYDSL|${LANGUAGE_UPPER}"
    "mydsl|${LANGUAGE_NAME}"
)

set(_root "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(_root "${_root}" ABSOLUTE)

# Files / folders the rename never touches.
set(_skip_pattern "^(build|\\.git|\\.cache|scripts/new-language\\.cmake)")

# --- 1. rewrite file contents -------------------------------------------
file(GLOB_RECURSE _files RELATIVE "${_root}" "${_root}/*")
foreach(_rel IN LISTS _files)
    if(_rel MATCHES "${_skip_pattern}")
        continue()
    endif()
    set(_abs "${_root}/${_rel}")
    if(IS_DIRECTORY "${_abs}")
        continue()
    endif()
    file(READ "${_abs}" _content)
    set(_orig "${_content}")
    foreach(_pair IN LISTS _token_pairs)
        string(REPLACE "|" ";" _pair "${_pair}")
        list(GET _pair 0 _from)
        list(GET _pair 1 _to)
        string(REPLACE "${_from}" "${_to}" _content "${_content}")
    endforeach()
    if(NOT "${_content}" STREQUAL "${_orig}")
        file(WRITE "${_abs}" "${_content}")
    endif()
endforeach()

function(_rename_leaf_in_place src)
    get_filename_component(_leaf "${src}" NAME)
    set(_new_leaf "${_leaf}")
    foreach(_pair IN LISTS _token_pairs)
        string(REPLACE "|" ";" _pair "${_pair}")
        list(GET _pair 0 _from)
        list(GET _pair 1 _to)
        string(REPLACE "${_from}" "${_to}" _new_leaf "${_new_leaf}")
    endforeach()
    if(NOT "${_leaf}" STREQUAL "${_new_leaf}")
        get_filename_component(_dir "${src}" DIRECTORY)
        file(RENAME "${src}" "${_dir}/${_new_leaf}")
    endif()
endfunction()

# --- 2a. rename file leaves (deepest first; only the basename matters) ---
foreach(_rel IN LISTS _files)
    if(_rel MATCHES "${_skip_pattern}")
        continue()
    endif()
    set(_src "${_root}/${_rel}")
    if(IS_DIRECTORY "${_src}")
        continue()
    endif()
    _rename_leaf_in_place("${_src}")
endforeach()

# --- 2b. rename directories, deepest first. After 2a all leaves under each
# directory already have their new basenames, so renaming the directory
# itself just moves the (already-renamed) subtree to its new location.
file(GLOB_RECURSE _paths LIST_DIRECTORIES TRUE
     RELATIVE "${_root}" "${_root}/*")
list(SORT _paths COMPARE NATURAL ORDER DESCENDING)
foreach(_rel IN LISTS _paths)
    if(_rel MATCHES "${_skip_pattern}")
        continue()
    endif()
    set(_src "${_root}/${_rel}")
    if(NOT IS_DIRECTORY "${_src}")
        continue()
    endif()
    _rename_leaf_in_place("${_src}")
endforeach()

message(STATUS
    "Renamed template to '${LANGUAGE_NAME}' "
    "(class=${LANGUAGE_CLASS}, ext=${FILE_EXT}).")
message(STATUS "Next: cmake -S . -B build && cmake --build build -j")
