#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/services/DefaultSharedCoreService.hpp>
#include <pegium/workspace/Documents.hpp>

namespace pegium::workspace {

class DefaultDocuments : public Documents,
                         protected services::DefaultSharedCoreService {
public:
  using services::DefaultSharedCoreService::DefaultSharedCoreService;

  void addDocument(std::shared_ptr<Document> document) override;

  [[nodiscard]] DocumentId getDocumentId(std::string_view uri) const override;

  [[nodiscard]] DocumentId getOrCreateDocumentId(std::string_view uri) override;

  [[nodiscard]] bool hasDocument(std::string_view uri) const override;

  [[nodiscard]] std::shared_ptr<Document>
  getDocument(std::string_view uri) const override;

  [[nodiscard]] std::shared_ptr<Document>
  getDocument(DocumentId id) const override;

  [[nodiscard]] std::string getDocumentUri(DocumentId id) const override;

  [[nodiscard]] std::vector<std::shared_ptr<Document>>
  getDocuments(std::string_view uri) const override;

  [[nodiscard]] std::shared_ptr<Document>
  getOrCreateDocument(std::string_view uri,
                      const utils::CancellationToken &cancelToken = {}) override;

  [[nodiscard]] std::shared_ptr<Document>
  createDocument(std::string uri, std::string text, std::string languageId = {},
                 const utils::CancellationToken &cancelToken = {}) override;

  [[nodiscard]] std::shared_ptr<Document>
  invalidateDocument(std::string_view uri) override;

  [[nodiscard]] std::shared_ptr<Document>
  deleteDocument(std::string_view uri) override;

  [[nodiscard]] std::shared_ptr<Document>
  deleteDocument(DocumentId id) override;

  [[nodiscard]] std::vector<std::shared_ptr<Document>>
  deleteDocuments(std::string_view uri) override;

  [[nodiscard]] utils::stream<std::shared_ptr<Document>> all() const override;

  void clear() override;

private:
  [[nodiscard]] static bool matches_uri_or_folder(std::string_view candidate,
                                                  std::string_view uri);
  [[nodiscard]] DocumentId ensureDocumentIdLocked(std::string_view uri);

  mutable std::mutex _mutex;
  std::unordered_map<std::string, std::shared_ptr<Document>> _documents;
  std::unordered_map<DocumentId, std::shared_ptr<Document>> _documentsById;
  std::unordered_map<std::string, DocumentId> _documentIdsByUri;
  std::unordered_map<DocumentId, std::string> _documentUrisById;
  DocumentId _nextDocumentId = 1;
};

} // namespace pegium::workspace
