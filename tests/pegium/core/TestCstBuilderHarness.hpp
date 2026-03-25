#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::test {

struct CstBuilderHarness {
  explicit CstBuilderHarness(workspace::Document &document)
      : root(text::TextSnapshot::copy(document.textDocument().getText())),
        builder(root) {
    root.attachDocument(document);
  }

  explicit CstBuilderHarness(std::string_view text)
      : root(text::TextSnapshot::copy(text)), builder(root) {}

  RootCstNode root;
  CstBuilder builder;
};

inline CstBuilderHarness makeCstBuilderHarness(workspace::Document &document) {
  return CstBuilderHarness(document);
}

inline CstBuilderHarness makeCstBuilderHarness(std::string_view text) {
  return CstBuilderHarness(text);
}

} // namespace pegium::test
