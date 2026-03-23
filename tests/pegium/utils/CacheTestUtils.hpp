#pragma once

#include <memory>
#include <span>
#include <stdexcept>
#include <string>

#include <pegium/CoreTestSupport.hpp>

namespace pegium::utils::test_support {

class AttachingDocumentFactory final : public workspace::DocumentFactory {
public:
  using workspace::DocumentFactory::attachTextDocument;

  [[nodiscard]] std::shared_ptr<workspace::Document> fromTextDocument(
      std::shared_ptr<workspace::TextDocument>,
      const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  [[nodiscard]] std::shared_ptr<workspace::Document>
  fromString(std::string, std::string_view,
             const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  [[nodiscard]] std::shared_ptr<workspace::Document>
  fromUri(std::string_view,
          const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  workspace::Document &
  update(workspace::Document &document,
         const utils::CancellationToken & = {}) const override {
    return document;
  }
};

inline std::unique_ptr<services::SharedCoreServices>
make_cache_shared_services(test::RecordingEventDocumentBuilder *&builder) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  builder = new test::RecordingEventDocumentBuilder();
  shared->workspace.documentBuilder.reset(builder);
  return shared;
}

inline std::shared_ptr<workspace::Document>
make_document(services::SharedCoreServices &sharedServices, std::string uri,
              std::string text = "content") {
  auto textDocument =
      test::make_text_document(std::move(uri), {}, std::move(text));
  auto document = std::make_shared<workspace::Document>(textDocument);
  sharedServices.workspace.documents->addDocument(document);
  return document;
}

inline void replace_document_text(workspace::Document &document, std::string text) {
  AttachingDocumentFactory factory;
  auto textDocument = std::make_shared<workspace::TextDocument>(document.textDocument());
  const workspace::TextDocumentContentChangeEvent change{.text = std::move(text)};
  (void)workspace::TextDocument::update(*textDocument,
                                        std::span(&change, std::size_t{1}),
                                        textDocument->version() + 1);
  factory.attachTextDocument(document, std::move(textDocument));
}

} // namespace pegium::utils::test_support
