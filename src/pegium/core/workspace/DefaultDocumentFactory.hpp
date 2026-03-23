#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/DefaultSharedCoreService.hpp>
#include <pegium/core/workspace/DocumentFactory.hpp>

namespace pegium::workspace {

/// Default factory creating managed documents from shared services and parsers.
class DefaultDocumentFactory : public DocumentFactory,
                               protected services::DefaultSharedCoreService {
public:
  using services::DefaultSharedCoreService::DefaultSharedCoreService;

  [[nodiscard]] std::shared_ptr<Document> fromTextDocument(
      std::shared_ptr<TextDocument> textDocument,
      const utils::CancellationToken &cancelToken = {}) const override;

  [[nodiscard]] std::shared_ptr<Document>
  fromString(std::string text, std::string uri,
             const utils::CancellationToken &cancelToken = {}) const override;

  [[nodiscard]] std::shared_ptr<Document>
  fromUri(std::string_view uri,
          const utils::CancellationToken &cancelToken = {}) const override;

  Document &update(
      Document &document,
      const utils::CancellationToken &cancelToken = {}) const override;

private:
  [[nodiscard]] std::shared_ptr<Document>
  createDocument(std::shared_ptr<TextDocument> textDocument,
                 const services::CoreServices &services,
                 const utils::CancellationToken &cancelToken) const;

  [[nodiscard]] std::shared_ptr<TextDocument>
  createTextDocument(std::string text, std::string uri, std::string languageId,
                     std::int64_t version) const;

  [[nodiscard]] std::shared_ptr<TextDocument>
  normalizeTextDocument(std::shared_ptr<TextDocument> textDocument,
                        std::string_view languageId = {}) const;

  void parse(Document &document, const services::CoreServices &services,
             const utils::CancellationToken &cancelToken) const;
};

} // namespace pegium::workspace
