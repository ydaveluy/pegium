#include <pegium/core/workspace/DefaultDocumentFactory.hpp>

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium::workspace {

namespace {

std::string_view previous_analyzed_text(const Document &document) {
  if (document.parseResult.cst != nullptr) {
    return document.parseResult.cst->getText();
  }
  return document.textDocument().getText();
}

} // namespace

std::shared_ptr<Document> DefaultDocumentFactory::fromTextDocument(
    std::shared_ptr<TextDocument> textDocument,
    const utils::CancellationToken &cancelToken) const {
  assert(textDocument != nullptr);
  textDocument = normalizeTextDocument(std::move(textDocument), {});
  const auto &services =
      shared.serviceRegistry->getServices(textDocument->uri());
  return createDocument(std::move(textDocument), services, cancelToken);
}

std::shared_ptr<Document> DefaultDocumentFactory::fromString(
    std::string text, std::string_view uri,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  const auto normalizedUri = utils::normalize_uri(uri);
  const auto &services = shared.serviceRegistry->getServices(normalizedUri);
  auto textDocument = createTextDocument(
      std::move(text), normalizedUri, services.languageMetaData.languageId, 0);
  return createDocument(std::move(textDocument), services, cancelToken);
}

std::shared_ptr<Document> DefaultDocumentFactory::fromUri(
    std::string_view uri, const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto normalizedUri = utils::normalize_uri(uri);
  if (const auto provider = shared.workspace.textDocuments;
      provider != nullptr) {
    if (auto textDocument = provider->get(normalizedUri); textDocument != nullptr) {
      const auto &services =
          shared.serviceRegistry->getServices(textDocument->uri());
      return createDocument(std::move(textDocument), services, cancelToken);
    }
  }

  const auto content =
      shared.workspace.fileSystemProvider->readFile(normalizedUri);
  const auto &services = shared.serviceRegistry->getServices(normalizedUri);
  auto textDocument = createTextDocument(
      content, normalizedUri, services.languageMetaData.languageId, 0);
  return createDocument(std::move(textDocument), services, cancelToken);
}

Document &DefaultDocumentFactory::update(
    Document &document, const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  const auto previousParsedText = previous_analyzed_text(document);
  if (document.uri.empty()) {
    throw utils::DocumentFactoryError("Cannot update a document without URI.");
  }

  const auto &services = shared.serviceRegistry->getServices(document.uri);
  std::shared_ptr<TextDocument> latestTextDocument;
  if (const auto provider = shared.workspace.textDocuments;
      provider != nullptr) {
    latestTextDocument = provider->get(document.uri);
  }

  if (latestTextDocument == nullptr) {
    const auto content =
        shared.workspace.fileSystemProvider->readFile(document.uri);
    latestTextDocument = createTextDocument(
        content, document.uri, services.languageMetaData.languageId, 0);
  }

  latestTextDocument =
      normalizeTextDocument(std::move(latestTextDocument),
                            services.languageMetaData.languageId);

  const auto textChanged = previousParsedText != latestTextDocument->getText();
  const auto needsParse =
      document.state < DocumentState::Parsed || textChanged;

  attachTextDocument(document, latestTextDocument);

  if (needsParse) {
    resetAnalysisState(document);
    parse(document, services, cancelToken);
  }

  document.state = DocumentState::Parsed;
  return document;
}

std::shared_ptr<Document> DefaultDocumentFactory::createDocument(
    std::shared_ptr<TextDocument> textDocument,
    const services::CoreServices &services,
    const utils::CancellationToken &cancelToken) const {
  assert(textDocument != nullptr);

  textDocument = normalizeTextDocument(
      std::move(textDocument), services.languageMetaData.languageId);

  auto document =
      std::make_shared<Document>(textDocument, textDocument->uri());
  parse(*document, services, cancelToken);
  document->state = DocumentState::Parsed;
  return document;
}

std::shared_ptr<TextDocument> DefaultDocumentFactory::createTextDocument(
    std::string text, std::string uri, std::string languageId,
    std::int64_t version) const {
  return std::make_shared<TextDocument>(TextDocument::create(
      std::move(uri), std::move(languageId), version, std::move(text)));
}

std::shared_ptr<TextDocument> DefaultDocumentFactory::normalizeTextDocument(
    std::shared_ptr<TextDocument> textDocument, std::string_view languageId) const {
  assert(textDocument != nullptr);

  const auto normalizedUri = utils::normalize_uri(textDocument->uri());
  const auto canonicalLanguageId =
      languageId.empty() ? textDocument->languageId() : std::string(languageId);

  if (normalizedUri == textDocument->uri() &&
      canonicalLanguageId == textDocument->languageId()) {
    return textDocument;
  }

  return std::make_shared<TextDocument>(TextDocument::create(
      normalizedUri, std::move(canonicalLanguageId), textDocument->version(),
      std::string(textDocument->getText())));
}

void DefaultDocumentFactory::parse(
    Document &document, const services::CoreServices &services,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  document.parseResult = services.parser->parse(snapshot(document.textDocument()),
                                                cancelToken);
  document.references = document.parseResult.references;
  if (document.parseResult.cst != nullptr) {
    document.parseResult.cst->attachDocument(document);
  }
}

} // namespace pegium::workspace
