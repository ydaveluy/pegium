#pragma once

#include <memory>
#include <string_view>

#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/TextDocument.hpp>

namespace pegium::workspace {

/// Read-only lookup interface for shared text documents.
class TextDocumentProvider {
public:
  virtual ~TextDocumentProvider() noexcept = default;

  /// Looks up the text document for an arbitrary URI, normalizing it first.
  [[nodiscard]] std::shared_ptr<TextDocument> get(std::string_view uri) const {
    return getNormalized(utils::normalize_uri(uri));
  }

  /// Looks up the text document for an already-normalized URI.
  ///
  /// Callers that already hold a normalized URI (e.g. `Document::uri`) use this
  /// to skip re-normalizing; passing a non-normalized URI will not match.
  [[nodiscard]] virtual std::shared_ptr<TextDocument>
  getNormalized(std::string_view normalizedUri) const = 0;
};

} // namespace pegium::workspace
