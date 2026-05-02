#pragma once

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::test {

inline void apply_parse_result(workspace::Document &document,
                               parser::ParseResult result) {
  document.parseResult = std::move(result);
  if (document.parseResult.cst != nullptr) {
    document.parseResult.cst->attachDocument(document);
  }
  if (document.parseResult.astArena != nullptr) {
    document.parseResult.astArena->attachDocument(document);
  }
}

} // namespace pegium::test
