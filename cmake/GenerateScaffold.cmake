# Run: cmake -DPEGIUM_TAG=<tag> -P cmake/GenerateScaffold.cmake
# Embeds templates/project/** into a self-contained docs/pegium-new.cmake.
cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED PEGIUM_TAG)
  set(PEGIUM_TAG "main")
endif()

set(_root "${CMAKE_CURRENT_LIST_DIR}/..")
set(_tpl_dir "${_root}/templates/project")
set(_out "${_root}/docs/pegium-new.cmake")

file(GLOB_RECURSE _files RELATIVE "${_tpl_dir}" "${_tpl_dir}/*")
list(SORT _files)

set(_body "")
foreach(_rel IN LISTS _files)
  file(READ "${_tpl_dir}/${_rel}" _content)
  # toggle: lsp/ vscode/ .vscode/ the release workflow cli/ are gated; else always
  set(_toggle "always")
  if(_rel MATCHES "(^|/)lsp/")
    set(_toggle "lsp")
  elseif(_rel MATCHES "(^|/)\\.?vscode/" OR _rel MATCHES "workflows/release")
    set(_toggle "vscode")
  elseif(_rel MATCHES "(^|/)cli/")
    set(_toggle "cli")
  endif()
  string(APPEND _body
    "_pegium_new_emit([==[${_rel}]==] [==[${_toggle}]==] [==[${_content}]==])\n")
endforeach()

configure_file("${CMAKE_CURRENT_LIST_DIR}/pegium-new.cmake.in" "${_out}" @ONLY)
file(READ "${_out}" _head)
file(WRITE "${_out}" "${_head}\n${_body}\n_pegium_new_finish()\n")
message(STATUS "Wrote ${_out} (${PEGIUM_TAG})")
