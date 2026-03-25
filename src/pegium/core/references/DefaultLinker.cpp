#include <pegium/core/references/DefaultLinker.hpp>

#include <cassert>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::references {

namespace {

workspace::NodeKey
make_node_key(const workspace::AstNodeDescription &description) {
  assert(description.documentId != workspace::InvalidDocumentId);
  assert(description.symbolId != workspace::InvalidSymbolId);
  return workspace::NodeKey{.documentId = description.documentId,
                            .symbolId = description.symbolId};
}

void log_reference_resolution_problem(
    const pegium::SharedCoreServices &shared,
    const workspace::Document *document, std::string message,
    std::optional<workspace::DocumentState> state = std::nullopt) {
  observability::Observation observation{
      .severity = observability::ObservationSeverity::Warning,
      .code = observability::ObservationCode::ReferenceResolutionProblem,
      .message = std::move(message),
      .state = state};
  if (document != nullptr) {
    observation.uri = document->uri;
    observation.languageId = document->textDocument().languageId();
    observation.documentId = document->id;
    if (!observation.state.has_value()) {
      observation.state = document->state;
    }
  }
  shared.observabilitySink->publish(observation);
}

} // namespace

DefaultLinker::DefaultLinker(const pegium::CoreServices &services)
    : pegium::DefaultCoreService(services) {}

void DefaultLinker::link(workspace::Document &document,
                         const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  if (!document.hasAst()) {
    return;
  }

  std::uint32_t cancelPollCounter = 0;
  for (const auto &handle : document.references) {
    if ((++cancelPollCounter & 0x3fU) == 0U) {
      utils::throw_if_cancelled(cancelToken);
    }
    const auto &reference = *handle.get();
    if (reference.isMultiReference()) {
      (void)static_cast<const AbstractMultiReference &>(reference)
          .resolvedDescriptionCount();
      continue;
    }
    (void)static_cast<const AbstractSingleReference &>(reference).resolve();
  }

  utils::throw_if_cancelled(cancelToken);
}

void DefaultLinker::unlink(workspace::Document &document) const {
  for (const auto &handle : document.references) {
    handle.get()->clearLinkState();
  }
}

workspace::AstNodeDescriptionOrError
DefaultLinker::getCandidate(const ReferenceInfo &reference) const {
  const auto *scopeProvider = services.references.scopeProvider.get();
  if (const auto *candidate = scopeProvider->getScopeEntry(reference);
      candidate != nullptr) {
    return *candidate;
  }
  return createLinkingError(reference);
}

workspace::LinkingError
DefaultLinker::createLinkingError(
    const ReferenceInfo &reference,
    std::optional<workspace::AstNodeDescription> targetDescription) const {
  const auto *document = reference.container == nullptr
                             ? nullptr
                             : std::addressof(getDocument(*reference.container));
  const bool retryable =
      document != nullptr &&
      document->state < workspace::DocumentState::ComputedScopes;
  if (retryable) {
    log_reference_resolution_problem(
        services.shared, document,
        "Attempted reference resolution before document reached "
        "ComputedScopes state (" +
            document->uri + ").",
        document->state);
  }

  auto message = std::string("Could not resolve reference");
  if (const auto type = reference.getReferenceType();
      type != std::type_index(typeid(void))) {
    const auto typeName = parser::detail::runtime_type_name(type);
    assert(!typeName.empty());
    message += " to " + typeName;
  }
  message += " named '" + std::string(reference.referenceText) + "'.";

  return workspace::LinkingError{
      .info = reference,
      .message = std::move(message),
      .targetDescription = std::move(targetDescription),
      .retryable = retryable};
}

workspace::AstNodeDescriptionsOrError
DefaultLinker::getCandidates(const ReferenceInfo &reference) const {
  const auto *scopeProvider = services.references.scopeProvider.get();
  std::vector<workspace::AstNodeDescription> descriptions;
  std::unordered_set<workspace::NodeKey, workspace::NodeKeyHash> seen;
  const auto collectDescription =
      [&descriptions, &seen](const workspace::AstNodeDescription &candidate) {
        if (!seen.insert(make_node_key(candidate)).second) {
          return true;
        }
        descriptions.push_back(candidate);
        return true;
      };
  (void)scopeProvider->visitScopeEntries(
      reference,
      utils::function_ref<bool(const workspace::AstNodeDescription &)>(
          collectDescription));
  if (descriptions.empty()) {
    return createLinkingError(reference);
  }
  return descriptions;
}

workspace::LinkingError
DefaultLinker::createCycleLinkingError(const ReferenceInfo &reference) const {
  std::string target = std::string(reference.getFeature());
  if (reference.container != nullptr) {
    const auto *locator = services.workspace.astNodeLocator.get();
    try {
      target = locator->getAstNodePath(*reference.container) + "/" +
               std::string(reference.getFeature());
    } catch (const std::exception &) {
      if (target.empty()) {
        target = "<unknown>";
      }
    }
  } else if (target.empty()) {
    target = "<unknown>";
  }

  return workspace::LinkingError{
      .info = reference,
      .message = "Cyclic reference resolution detected: " + target +
                 " (symbol '" + std::string(reference.referenceText) + "')",
      .targetDescription = std::nullopt,
      .retryable = false};
}

workspace::LinkingError DefaultLinker::createExceptionLinkingError(
    const ReferenceInfo &reference, const std::string &message) const {
  auto fullMessage = "An error occurred while resolving reference to '" +
                     std::string(reference.referenceText) + "': " + message;
  log_reference_resolution_problem(
      services.shared,
      reference.container == nullptr
          ? nullptr
          : std::addressof(getDocument(*reference.container)),
      fullMessage);
  return workspace::LinkingError{
      .info = reference,
      .message = std::move(fullMessage),
      .targetDescription = std::nullopt,
      .retryable = false};
}

workspace::ResolvedAstNodeDescriptionOrError
DefaultLinker::getLinkedNode(const ReferenceInfo &reference,
                             const workspace::Document &currentDocument) const {
  const auto *scopeProvider = services.references.scopeProvider.get();
  if (const auto *candidate = scopeProvider->getScopeEntry(reference);
      candidate != nullptr) {
    return workspace::ResolvedAstNodeDescription{
        .node = std::addressof(workspace::resolve_ast_node(
            *services.shared.workspace.documents, *candidate,
            currentDocument)),
        .description = *candidate};
  }
  return createLinkingError(reference);
}

workspace::ResolvedAstNodeDescriptionsOrError
DefaultLinker::getLinkedNodes(const ReferenceInfo &reference,
                              const workspace::Document &currentDocument) const {
  std::vector<workspace::ResolvedAstNodeDescription> resolved;
  std::optional<workspace::AstNodeDescription> firstCandidate;
  std::unordered_set<workspace::NodeKey, workspace::NodeKeyHash> seen;
  const auto *scopeProvider = services.references.scopeProvider.get();
  const auto resolveCandidate =
      [this, &currentDocument, &resolved, &firstCandidate,
       &seen](const workspace::AstNodeDescription &candidate) {
        if (!seen.insert(make_node_key(candidate)).second) {
          return true;
        }
        if (!firstCandidate.has_value()) {
          firstCandidate = candidate;
        }
        resolved.push_back(
            {.node = std::addressof(workspace::resolve_ast_node(
                 *services.shared.workspace.documents, candidate,
                 currentDocument)),
             .description = candidate});
        return true;
      };
  (void)scopeProvider->visitScopeEntries(
      reference,
      utils::function_ref<bool(const workspace::AstNodeDescription &)>(
          resolveCandidate));
  if (!resolved.empty()) {
    return resolved;
  }
  return createLinkingError(reference, std::move(firstCandidate));
}

workspace::ResolvedAstNodeDescriptionOrError
DefaultLinker::resolve(const AbstractSingleReference &reference) const {
  const auto info = makeReferenceInfo(reference);
  try {
    assert(reference.getContainer() != nullptr);
    const auto &currentDocument = getDocument(*reference.getContainer());
    return getLinkedNode(info, currentDocument);
  } catch (const CyclicReferenceResolution &cycle) {
    return createCycleLinkingError(makeReferenceInfo(cycle.reference()));
  } catch (const std::exception &error) {
    return createExceptionLinkingError(info, error.what());
  }
}

workspace::ResolvedAstNodeDescriptionsOrError
DefaultLinker::resolveAll(const AbstractMultiReference &reference) const {
  const auto info = makeReferenceInfo(reference);
  try {
    assert(reference.getContainer() != nullptr);
    const auto &currentDocument = getDocument(*reference.getContainer());
    return getLinkedNodes(info, currentDocument);
  } catch (const CyclicReferenceResolution &cycle) {
    return createCycleLinkingError(makeReferenceInfo(cycle.reference()));
  } catch (const std::exception &error) {
    return createExceptionLinkingError(info, error.what());
  }
}

} // namespace pegium::references
