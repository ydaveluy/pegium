#pragma once

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::test {

inline void apply_parse_result(workspace::Document &document,
                               parser::ParseResult result) {
  document.parseResult = std::move(result);
  document.references = document.parseResult.references;
  if (document.parseResult.cst != nullptr) {
    document.parseResult.cst->attachDocument(document);
  }
}

} // namespace pegium::test
