#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/TextDocument.hpp>

namespace pegium::workspace {

class DocumentFactory {
public:
  virtual ~DocumentFactory() noexcept = default;

  [[nodiscard]] virtual std::shared_ptr<Document> fromTextDocument(
      std::shared_ptr<const TextDocument> textDocument,
      const utils::CancellationToken &cancelToken = {}) const = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  fromString(std::string text, std::string uri, std::string languageId = {},
             std::optional<std::int64_t> clientVersion = std::nullopt,
             const utils::CancellationToken &cancelToken = {}) const = 0;

  [[nodiscard]] virtual std::shared_ptr<Document>
  fromUri(std::string_view uri,
          const utils::CancellationToken &cancelToken = {}) const = 0;

  virtual Document &update(
      Document &document,
      const utils::CancellationToken &cancelToken = {}) const = 0;
};

} // namespace pegium::workspace
