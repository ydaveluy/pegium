#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/LspTestSupport.hpp>

namespace pegium::test {

template <typename ParserT>
std::shared_ptr<workspace::Document>
parse_document(ParserT &parser, std::string text, std::string uri = {},
               std::string languageId = {}) {
  auto document = std::make_shared<workspace::Document>();
  document->uri = std::move(uri);
  document->languageId = std::move(languageId);
  document->replaceText(std::move(text));
  parser.parse(*document);
  return document;
}

inline bool has_diagnostic_message(const workspace::Document &document,
                                   std::string_view needle) {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace pegium::test
