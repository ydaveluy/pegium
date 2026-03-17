#pragma once

#include <vector>

#include <pegium/parser/Parser.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::workspace {

class ParseDiagnosticsExtractor {
public:
  explicit ParseDiagnosticsExtractor(
      const Document &document,
      const parser::Parser *parser = nullptr) noexcept
      : _document(document), _parser(parser) {}

  [[nodiscard]] std::vector<services::Diagnostic>
  extract(std::vector<services::Diagnostic> validationDiagnostics = {},
          const utils::CancellationToken &cancelToken = {}) const;

private:
  const Document &_document;
  const parser::Parser *_parser = nullptr;
};

} // namespace pegium::workspace
