#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {

/// Registry of managed workspace documents indexed by URI and identifier.
class Documents {
public:
  virtual ~Documents() noexcept = default;

  /// Stores a managed document. `document` must be non-null.
  virtual void addDocument(std::shared_ptr<Document> document) = 0;

  [[nodiscard]] virtual DocumentId
  getDocumentId(std::string_view uri) const = 0;

  [[nodiscard]] virtual DocumentId
  getOrCreateDocumentId(std::string_view uri) = 0;

  [[nodiscard]] virtual bool hasDocument(std::string_view uri) const = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  getDocument(std::string_view uri) const = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  getDocument(DocumentId id) const = 0;

  [[nodiscard]] virtual std::string
  getDocumentUri(DocumentId id) const = 0;

  [[nodiscard]] virtual std::vector<std::shared_ptr<Document>>
  getDocuments(std::string_view uri) const = 0;

  /// Returns the existing managed document for `uri`, or creates it.
  ///
  /// Returns a non-null shared handle on success and throws if the URI cannot
  /// be materialized as a managed document.
  [[nodiscard]] virtual std::shared_ptr<Document>
  getOrCreateDocument(std::string_view uri,
                      const utils::CancellationToken &cancelToken = {}) = 0;

  /// Creates and stores a managed document for the given text and URI.
  ///
  /// Returns a non-null shared handle on success and throws if the URI cannot
  /// be materialized as a managed document.
  [[nodiscard]] virtual std::shared_ptr<Document>
  createDocument(std::string uri, std::string text,
                 const utils::CancellationToken &cancelToken = {}) = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  deleteDocument(std::string_view uri) = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  deleteDocument(DocumentId id) = 0;

  [[nodiscard]] virtual std::vector<std::shared_ptr<Document>>
  deleteDocuments(std::string_view uri) = 0;

  [[nodiscard]] virtual std::vector<std::shared_ptr<Document>> all() const = 0;
};

} // namespace pegium::workspace
