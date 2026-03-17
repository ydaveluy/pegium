#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::workspace {

class Documents {
public:
  virtual ~Documents() noexcept = default;

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

  [[nodiscard]] virtual std::shared_ptr<Document>
  getOrCreateDocument(std::string_view uri,
                      const utils::CancellationToken &cancelToken = {}) = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  createDocument(std::string uri, std::string text, std::string languageId = {},
                 const utils::CancellationToken &cancelToken = {}) = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  invalidateDocument(std::string_view uri) = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  deleteDocument(std::string_view uri) = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  deleteDocument(DocumentId id) = 0;

  [[nodiscard]] virtual std::vector<std::shared_ptr<Document>>
  deleteDocuments(std::string_view uri) = 0;

  [[nodiscard]] virtual utils::stream<std::shared_ptr<Document>> all() const = 0;

  virtual void clear() = 0;
};

} // namespace pegium::workspace
