#pragma once

#include <memory>
#include <string>

#include <pegium/CoreTestSupport.hpp>

namespace pegium::utils::test_support {

inline std::unique_ptr<services::SharedCoreServices>
make_cache_shared_services(test::RecordingEventDocumentBuilder *&builder) {
  auto shared = test::make_shared_core_services();
  builder = new test::RecordingEventDocumentBuilder();
  shared->workspace.documentBuilder.reset(builder);
  return shared;
}

inline std::shared_ptr<workspace::Document>
make_document(services::SharedCoreServices &sharedServices, std::string uri,
              std::string text = "content") {
  auto document = std::make_shared<workspace::Document>();
  document->uri = std::move(uri);
  document->replaceText(std::move(text));
  sharedServices.workspace.documents->addDocument(document);
  return document;
}

} // namespace pegium::utils::test_support
