#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/syntax-tree/CstBuilder.hpp>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/syntax-tree/RootCstNode.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::test {

struct CstBuilderHarness {
  explicit CstBuilderHarness(workspace::Document &document)
      : CstBuilderHarness(nullptr, document) {}

  explicit CstBuilderHarness(std::string_view text)
      : CstBuilderHarness([text] {
          auto document = std::make_shared<workspace::Document>();
          document->setText(std::string{text});
          return document;
        }()) {}

  RootCstNode root;
  CstBuilder builder;

private:
  explicit CstBuilderHarness(std::shared_ptr<workspace::Document> ownedDocument)
      : CstBuilderHarness(ownedDocument, *ownedDocument) {}

  CstBuilderHarness(std::shared_ptr<workspace::Document> ownedDocument,
                    workspace::Document &document)
      : _ownedDocument(std::move(ownedDocument)), root(document), builder(root) {
  }

  std::shared_ptr<workspace::Document> _ownedDocument = nullptr;
};

inline CstBuilderHarness makeCstBuilderHarness(workspace::Document &document) {
  return CstBuilderHarness(document);
}

inline CstBuilderHarness makeCstBuilderHarness(std::string_view text) {
  return CstBuilderHarness(text);
}

} // namespace pegium::test
