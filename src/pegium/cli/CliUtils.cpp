#include <pegium/cli/CliUtils.hpp>

#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/DocumentBuilder.hpp>
#include <pegium/core/workspace/Documents.hpp>

namespace {

std::string normalize_extension(std::string_view extension) {
  if (extension.empty()) {
    return {};
  }
  std::string normalized(extension);
  if (normalized.front() != '.') {
    normalized.insert(normalized.begin(), '.');
  }
  return normalized;
}

std::string join_values(const std::vector<std::string> &values) {
  std::string out;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out += ", ";
    }
    out += values[index];
  }
  return out;
}

bool matches_language_path(
    const std::filesystem::path &path,
    const pegium::services::LanguageMetaData &languageMetaData) {
  if (languageMetaData.fileExtensions.empty() &&
      languageMetaData.fileNames.empty()) {
    return true;
  }

  const auto fileName = path.filename().string();
  for (const auto &candidate : languageMetaData.fileNames) {
    if (candidate == fileName) {
      return true;
    }
  }

  const auto extension = normalize_extension(path.extension().string());
  for (const auto &candidate : languageMetaData.fileExtensions) {
    if (normalize_extension(candidate) == extension) {
      return true;
    }
  }

  return false;
}

void validate_language_path(
    const std::filesystem::path &path,
    const pegium::services::LanguageMetaData &languageMetaData) {
  if (matches_language_path(path, languageMetaData)) {
    return;
  }

  if (!languageMetaData.fileNames.empty() &&
      !languageMetaData.fileExtensions.empty()) {
    throw std::invalid_argument(
        "Please choose a file with one of these names: " +
        join_values(languageMetaData.fileNames) +
        " or one of these extensions: " +
        join_values(languageMetaData.fileExtensions) + ".");
  }

  if (!languageMetaData.fileNames.empty()) {
    throw std::invalid_argument(
        "Please choose a file with one of these names: " +
        join_values(languageMetaData.fileNames) + ".");
  }

  throw std::invalid_argument(
      "Please choose a file with one of these extensions: " +
      join_values(languageMetaData.fileExtensions) + ".");
}

} // namespace

namespace pegium::cli {

SharedServices make_shared_services() {
  SharedServices sharedServices;
  services::installDefaultSharedCoreServices(sharedServices);
  sharedServices.workspace.configurationProvider->initialize(
      workspace::InitializeParams{});
  auto initialized =
      sharedServices.workspace.configurationProvider->initialized(
          workspace::InitializedParams{});
  initialized.get();
  return sharedServices;
}

std::shared_ptr<workspace::Document>
build_document_from_path(std::string_view path,
                         const services::CoreServices &services,
                         bool validation) {
  const auto absolutePath = std::filesystem::absolute(std::filesystem::path(path));
  const auto absolutePathString = absolutePath.string();
  const auto uri = utils::path_to_file_uri(absolutePathString);

  if (!services.shared.workspace.fileSystemProvider->exists(uri)) {
    throw std::invalid_argument("File " + absolutePathString +
                                " does not exist.");
  }

  validate_language_path(absolutePath, services.languageMetaData);

  const auto *registeredServices = services.shared.serviceRegistry->findServices(uri);
  if (registeredServices == nullptr) {
    throw std::runtime_error(
        "Language services for '" + services.languageMetaData.languageId +
        "' must be registered before loading CLI documents.");
  }

  if (registeredServices != &services) {
    throw std::runtime_error(
        "Language services for '" + services.languageMetaData.languageId +
        "' are not registered for '" + absolutePathString + "'.");
  }

  auto document = services.shared.workspace.documents->getOrCreateDocument(uri);

  workspace::BuildOptions buildOptions;
  buildOptions.validation = validation;
  services.shared.workspace.documentBuilder->build(
      std::span<const std::shared_ptr<workspace::Document>>(&document, 1),
      buildOptions);
  return document;
}

bool has_error_diagnostics(const workspace::Document &document) noexcept {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.severity == services::DiagnosticSeverity::Error) {
      return true;
    }
  }
  return false;
}

void print_error_diagnostics(const workspace::Document &document,
                             std::ostream &out) {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.severity != services::DiagnosticSeverity::Error) {
      continue;
    }

    const auto position =
        document.textDocument().positionAt(diagnostic.begin);
    out << "line " << position.line << ": " << diagnostic.message;
    if (diagnostic.end > diagnostic.begin &&
        diagnostic.end <= document.textDocument().getText().size()) {
      out << " ["
          << document.textDocument().getText().substr(
                 diagnostic.begin, diagnostic.end - diagnostic.begin)
          << "]";
    }
    out << '\n';
  }
}

} // namespace pegium::cli
