#pragma once

#include <memory>
#include <string_view>

#include <pegium/core/workspace/TextDocument.hpp>

namespace pegium::workspace {

/// Read-only lookup interface for shared text documents.
class TextDocumentProvider {
public:
  virtual ~TextDocumentProvider() noexcept = default;

  [[nodiscard]] virtual std::shared_ptr<TextDocument>
  get(std::string_view uri) const = 0;
};

} // namespace pegium::workspace
