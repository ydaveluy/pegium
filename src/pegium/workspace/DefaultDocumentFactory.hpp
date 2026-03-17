#pragma once

#include <utility>

#include <pegium/services/DefaultSharedCoreService.hpp>
#include <pegium/workspace/DocumentFactory.hpp>

namespace pegium::workspace {

class DefaultDocumentFactory : public DocumentFactory,
                               protected services::DefaultSharedCoreService {
public:
  using services::DefaultSharedCoreService::DefaultSharedCoreService;

  [[nodiscard]] std::shared_ptr<Document> fromTextDocument(
      std::shared_ptr<const TextDocument> textDocument,
      const utils::CancellationToken &cancelToken = {}) const override;

  [[nodiscard]] std::shared_ptr<Document>
  fromString(std::string text, std::string uri, std::string languageId = {},
             std::optional<std::int64_t> clientVersion = std::nullopt,
             const utils::CancellationToken &cancelToken = {}) const override {
    return fromTextDocument(createTextDocument(std::move(text), std::move(uri),
                                               std::move(languageId),
                                               clientVersion),
                            cancelToken);
  }

  [[nodiscard]] std::shared_ptr<Document>
  fromUri(std::string_view uri,
          const utils::CancellationToken &cancelToken = {}) const override;

  Document &update(
      Document &document,
      const utils::CancellationToken &cancelToken = {}) const override;

private:
  [[nodiscard]] std::shared_ptr<const TextDocument>
  createTextDocument(std::string text, std::string uri, std::string languageId,
                     std::optional<std::int64_t> clientVersion) const;

  [[nodiscard]] std::string resolveLanguageId(
      std::string_view uri, std::string languageId = {}) const;

  void parse(Document &document,
             const utils::CancellationToken &cancelToken) const;
};

} // namespace pegium::workspace
