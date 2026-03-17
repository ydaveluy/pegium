#include <pegium/validation/DefaultDocumentValidator.hpp>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/services/CoreServices.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/validation/ValidationRegistry.hpp>

namespace pegium::validation {

namespace {

ValidationAcceptor
make_collecting_acceptor(std::vector<services::Diagnostic> &diagnostics,
                         const std::string &source) {
  return [&diagnostics, &source](services::Diagnostic diagnostic) {
    if (diagnostic.source.empty()) {
      diagnostic.source = source;
    }
    diagnostics.push_back(std::move(diagnostic));
  };
}

} // namespace

bool DefaultDocumentValidator::run_builtin_validation(
    const ValidationOptions &options) const noexcept {
  return options.categories.empty() ||
         std::ranges::find(options.categories, kBuiltInValidationCategory) !=
             options.categories.end();
}

bool DefaultDocumentValidator::run_custom_validation(
    const ValidationOptions &options) const noexcept {
  if (options.categories.empty()) {
    return true;
  }
  return std::ranges::any_of(options.categories, [](const std::string &category) {
    return category != kBuiltInValidationCategory;
  });
}

std::vector<services::Diagnostic> DefaultDocumentValidator::validateDocument(
    const workspace::Document &document, const ValidationOptions &options,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  if (!options.enabled) {
    return {};
  }

  std::vector<services::Diagnostic> diagnostics;
  const auto source = coreServices.languageId.empty()
                          ? std::string("validation")
                          : std::string(coreServices.languageId);

  if (run_builtin_validation(options)) {
    std::uint32_t cancelPollCounter = 0;
    for (const auto &handle : document.references) {
      if ((++cancelPollCounter & 0x3fU) == 0U) {
        utils::throw_if_cancelled(cancelToken);
      }
      const auto *reference = handle.getConst();
      if (reference == nullptr || !reference->hasError()) {
        continue;
      }

      const auto refText = reference->getRefText();
      if (refText.empty()) {
        continue;
      }

      TextOffset begin = 0;
      TextOffset end = static_cast<TextOffset>(refText.size());
      if (const auto refNode = reference->getRefNode(); refNode.has_value()) {
        begin = refNode->getBegin();
        end = refNode->getEnd();
      }

      diagnostics.push_back(services::Diagnostic{
          .severity = services::DiagnosticSeverity::Error,
          .message = "Unresolved reference: " + std::string(refText),
          .source = source,
          .code =
              services::DiagnosticCode(std::string("linking.unresolved-reference")),
          .data = std::nullopt,
          .begin = begin,
          .end = end});
    }
  }

  const auto *validationRegistry =
      coreServices.validation.validationRegistry.get();
  if (validationRegistry == nullptr || document.parseResult.value == nullptr ||
      !run_custom_validation(options)) {
    return diagnostics;
  }

  const ValidationAcceptor acceptor = make_collecting_acceptor(diagnostics, source);

  for (const auto &checkBefore : validationRegistry->checksBefore()) {
    utils::throw_if_cancelled(cancelToken);
    checkBefore(*document.parseResult.value, acceptor, options.categories,
                cancelToken);
  }

  validationRegistry->runChecks(*document.parseResult.value, options.categories,
                                acceptor);
  std::uint32_t cancelPollCounter = 0;
  for (const auto *node : document.parseResult.value->getAllContent()) {
    if ((++cancelPollCounter & 0x3fU) == 0U) {
      utils::throw_if_cancelled(cancelToken);
    }
    if (node == nullptr) {
      continue;
    }
    validationRegistry->runChecks(*node, options.categories, acceptor);
  }

  for (const auto &checkAfter : validationRegistry->checksAfter()) {
    utils::throw_if_cancelled(cancelToken);
    checkAfter(*document.parseResult.value, acceptor, options.categories,
               cancelToken);
  }

  utils::throw_if_cancelled(cancelToken);
  return diagnostics;
}

} // namespace pegium::validation
