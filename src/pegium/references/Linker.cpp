#include <pegium/references/Linker.hpp>

#include <pegium/utils/Cancellation.hpp>

namespace pegium::references {

ReferenceResolution resolveReference(const Linker &linker,
                                     const AbstractReference &reference) {
  return linker.resolve(reference);
}

MultiReferenceResolution resolveAllReferences(
    const Linker &linker, const AbstractReference &reference) {
  return linker.resolveAll(reference);
}

void Linker::unlink(workspace::Document &document,
                    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  for (const auto &handle : document.references) {
    auto *reference = handle.get();
    if (reference != nullptr) {
      reference->clearLinkState();
    }
  }
  document.referenceDescriptions.clear();
  utils::throw_if_cancelled(cancelToken);
}

workspace::AstNodeDescriptionOrError
Linker::getCandidate(const ReferenceInfo &reference) const {
  return workspace::LinkingError{
      .info = reference,
      .message = "Linker#getCandidate is not implemented for this language.",
      .targetDescription = std::nullopt};
}

workspace::AstNodeDescriptionsOrError
Linker::getCandidates(const ReferenceInfo &reference) const {
  return workspace::LinkingError{
      .info = reference,
      .message = "Linker#getCandidates is not implemented for this language.",
      .targetDescription = std::nullopt};
}

ReferenceResolution Linker::resolve(const AbstractReference &reference) const {
  const auto candidate = getCandidate(makeReferenceInfo(reference));
  if (const auto *error = std::get_if<workspace::LinkingError>(&candidate);
      error != nullptr) {
    return ReferenceResolution{.node = nullptr,
                               .description = nullptr,
                               .errorMessage = error->message};
  }
  return ReferenceResolution{
      .node = nullptr,
      .description = nullptr,
      .errorMessage = "Linker#resolve is not implemented for this language."};
}

MultiReferenceResolution Linker::resolveAll(
    const AbstractReference &reference) const {
  const auto candidates = getCandidates(makeReferenceInfo(reference));
  if (const auto *error = std::get_if<workspace::LinkingError>(&candidates);
      error != nullptr) {
    return MultiReferenceResolution{.items = {}, .errorMessage = error->message};
  }
  return MultiReferenceResolution{
      .items = {},
      .errorMessage = "Linker#resolveAll is not implemented for this language."};
}

} // namespace pegium::references
