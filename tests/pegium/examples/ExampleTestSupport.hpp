#pragma once

#include <filesystem>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

#include <pegium/core/ParseSupport.hpp>
#include <pegium/lsp/LspTestSupport.hpp>

namespace pegium::test {

template <typename ParserT>
std::shared_ptr<workspace::Document>
parse_document(ParserT &parser, std::string text, std::string uri = {},
               std::string languageId = {}) {
  auto document = std::make_shared<workspace::Document>(
      make_text_document(std::move(uri), std::move(languageId),
                         std::move(text)));
  apply_parse_result(*document,
                     parser.parse(document->textDocument().getText()));
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

[[nodiscard]] inline std::filesystem::path current_source_path(
    std::source_location location = std::source_location::current()) {
  return location.file_name();
}

[[nodiscard]] inline std::filesystem::path current_source_directory(
    std::source_location location = std::source_location::current()) {
  return current_source_path(location).parent_path();
}

} // namespace pegium::test
