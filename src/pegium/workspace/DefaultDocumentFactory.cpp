#include <pegium/workspace/DefaultDocumentFactory.hpp>

#include <utility>

#include <pegium/services/CoreServices.hpp>
#include <pegium/services/ServiceRegistry.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/utils/UriUtils.hpp>

namespace pegium::workspace {

std::shared_ptr<Document> DefaultDocumentFactory::fromTextDocument(
    std::shared_ptr<const TextDocument> textDocument,
    const utils::CancellationToken &cancelToken) const {
  if (textDocument == nullptr) {
    return nullptr;
  }

  auto document = std::make_shared<Document>();
  document->setTextDocument(textDocument);
  if (!textDocument->uri.empty()) {
    document->uri = textDocument->uri;
  }
  if (!textDocument->languageId.empty()) {
    document->languageId = textDocument->languageId;
  } else {
    document->languageId = resolveLanguageId(document->uri);
  }
  parse(*document, cancelToken);
  document->state = DocumentState::Parsed;
  return document;
}

std::shared_ptr<Document> DefaultDocumentFactory::fromUri(
    std::string_view uri, const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto normalizedUri = utils::normalize_uri(uri);
  if (sharedCoreServices.workspace.textDocuments != nullptr) {
    if (auto textDocument =
            sharedCoreServices.workspace.textDocuments->get(normalizedUri);
        textDocument != nullptr) {
      return fromTextDocument(std::move(textDocument), cancelToken);
    }
  }

  if (sharedCoreServices.workspace.fileSystemProvider == nullptr) {
    return nullptr;
  }
  const auto content =
      sharedCoreServices.workspace.fileSystemProvider->readFile(normalizedUri);
  if (!content.has_value()) {
    return nullptr;
  }

  return fromString(*content, normalizedUri, resolveLanguageId(normalizedUri),
                    std::nullopt, cancelToken);
}

Document &DefaultDocumentFactory::update(
    Document &document, const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  const auto previousTextDocument = document.textDocument();
  std::shared_ptr<const TextDocument> latestTextDocument;
  if (!document.uri.empty() &&
      sharedCoreServices.workspace.textDocuments != nullptr) {
    document.uri = utils::normalize_uri(document.uri);
    latestTextDocument =
        sharedCoreServices.workspace.textDocuments->get(document.uri);
  }

  if (latestTextDocument == nullptr && !document.uri.empty() &&
      sharedCoreServices.workspace.fileSystemProvider != nullptr) {
    if (const auto content =
            sharedCoreServices.workspace.fileSystemProvider->readFile(
                document.uri);
        content.has_value()) {
      latestTextDocument = createTextDocument(
          *content, document.uri,
          resolveLanguageId(document.uri, document.languageId), std::nullopt);
    }
  }

  if (latestTextDocument == nullptr) {
    latestTextDocument = document.textDocument();
  }

  if (latestTextDocument == nullptr) {
    latestTextDocument = createTextDocument(
        {}, document.uri, resolveLanguageId(document.uri, document.languageId),
        std::nullopt);
  }

  const auto textChanged =
      previousTextDocument == nullptr ||
      previousTextDocument->text() != latestTextDocument->text() ||
      previousTextDocument->languageId != latestTextDocument->languageId;

  document.setTextDocument(latestTextDocument);
  if (!latestTextDocument->languageId.empty()) {
    document.languageId = latestTextDocument->languageId;
  } else if (document.languageId.empty()) {
    document.languageId = resolveLanguageId(document.uri);
  }

  if (!textChanged && document.state < DocumentState::Parsed) {
    document.state = DocumentState::Parsed;
    return document;
  }

  if (document.state < DocumentState::Parsed) {
    parse(document, cancelToken);
    document.state = DocumentState::Parsed;
  }
  return document;
}

std::shared_ptr<const TextDocument> DefaultDocumentFactory::createTextDocument(
    std::string text, std::string uri, std::string languageId,
    std::optional<std::int64_t> clientVersion) const {
  auto textDocument = std::make_shared<TextDocument>();
  textDocument->uri = utils::normalize_uri(uri);
  textDocument->languageId = std::move(languageId);
  textDocument->replaceText(std::move(text));
  textDocument->setClientVersion(clientVersion);
  return textDocument;
}

std::string DefaultDocumentFactory::resolveLanguageId(
    std::string_view uri, std::string languageId) const {
  if (!languageId.empty()) {
    return languageId;
  }
  if (sharedCoreServices.workspace.textDocuments != nullptr) {
    if (auto textDocument = sharedCoreServices.workspace.textDocuments->get(uri);
        textDocument != nullptr && !textDocument->languageId.empty()) {
      return textDocument->languageId;
    }
  }
  if (sharedCoreServices.serviceRegistry == nullptr) {
    return {};
  }
  const auto *services = sharedCoreServices.serviceRegistry->getServices(uri);
  return services == nullptr ? std::string{} : services->languageId;
}

void DefaultDocumentFactory::parse(
    Document &document, const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  if (sharedCoreServices.serviceRegistry == nullptr ||
      document.languageId.empty()) {
    return;
  }
  const auto *services =
      sharedCoreServices.serviceRegistry->getServicesByLanguageId(
          document.languageId);
  if (services == nullptr || services->parser == nullptr) {
    return;
  }
  services->parser->parse(document, cancelToken);
}

} // namespace pegium::workspace
