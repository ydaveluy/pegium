#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/TextDocument.hpp>

namespace pegium::workspace {

/// Creates managed `Document` instances from text or URIs.
class DocumentFactory {
public:
  virtual ~DocumentFactory() noexcept = default;

  /// Creates a managed document from an existing text document.
  ///
  /// `textDocument` must be non-null.
  ///
  /// Returns a non-null shared handle on success and throws if its URI does
  /// not resolve to registered services.
  [[nodiscard]] virtual std::shared_ptr<Document> fromTextDocument(
      std::shared_ptr<TextDocument> textDocument,
      const utils::CancellationToken &cancelToken = {}) const = 0;

  /// Creates a managed document from in-memory text.
  ///
  /// Returns a non-null shared handle on success and throws if the URI does
  /// not resolve to registered services. The returned document always carries
  /// a normalized non-empty URI.
  [[nodiscard]] virtual std::shared_ptr<Document>
  fromString(std::string text, std::string_view uri,
             const utils::CancellationToken &cancelToken = {}) const = 0;

  /// Creates a managed document from the latest text available for a URI.
  ///
  /// Returns a non-null shared handle on success and throws if the URI does
  /// not resolve to registered services or cannot be loaded. The returned
  /// document always carries a normalized non-empty URI.
  [[nodiscard]] virtual std::shared_ptr<Document>
  fromUri(std::string_view uri,
          const utils::CancellationToken &cancelToken = {}) const = 0;

  virtual Document &update(
      Document &document,
      const utils::CancellationToken &cancelToken = {}) const = 0;

protected:
  /// Replaces the attached text snapshot while preserving document identity.
  ///
  /// Intended only for managed document lifecycle implementations.
  void attachTextDocument(
      Document &document,
      std::shared_ptr<TextDocument> textDocument) const {
    document.attachTextDocument(std::move(textDocument));
  }

  /// Resets the derived analysis state of `document`.
  void resetAnalysisState(Document &document) const noexcept {
    document.resetAnalysisState();
  }

  /// Returns the immutable snapshot backing `textDocument`.
  [[nodiscard]] text::TextSnapshot
  snapshot(const TextDocument &textDocument) const noexcept {
    return textDocument.snapshot();
  }
};

} // namespace pegium::workspace
