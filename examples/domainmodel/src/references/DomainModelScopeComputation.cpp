#include "references/DomainModelScopeComputation.hpp"

#include <pegium/services/CoreServices.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace domainmodel::services::references {
namespace {

using namespace domainmodel::ast;

std::string type_name(const Type &type) {
  if (const auto *entity = dynamic_cast<const Entity *>(&type)) {
    return entity->name;
  }
  if (const auto *dataType = dynamic_cast<const DataType *>(&type)) {
    return dataType->name;
  }
  return {};
}

template <typename Container>
const std::vector<pegium::AstNode::pointer<AbstractElement>> &
elements_of(const Container &container) {
  return container.elements;
}

} // namespace

DomainModelScopeComputation::DomainModelScopeComputation(
    const pegium::services::CoreServices &services,
    std::shared_ptr<const QualifiedNameProvider> qualifiedNameProvider)
    : pegium::references::DefaultScopeComputation(services),
      _qualifiedNameProvider(std::move(qualifiedNameProvider)) {}

std::vector<pegium::workspace::AstNodeDescription>
DomainModelScopeComputation::collectExportedSymbols(
    const pegium::workspace::Document &document,
    const pegium::utils::CancellationToken &cancelToken) const {
  std::vector<pegium::workspace::AstNodeDescription> symbols;
  const auto *model =
      pegium::ast_ptr_cast<DomainModel>(document.parseResult.value);
  if (model == nullptr) {
    return symbols;
  }

  collectExportedSymbols(model->elements, {}, document, symbols, cancelToken);
  return symbols;
}

void DomainModelScopeComputation::collectExportedSymbols(
    const std::vector<pegium::AstNode::pointer<AbstractElement>> &elements,
    std::string_view qualifier, const pegium::workspace::Document &document,
    std::vector<pegium::workspace::AstNodeDescription> &symbols,
    const pegium::utils::CancellationToken &cancelToken) const {
  const auto *descriptionProvider =
      coreServices.workspace.astNodeDescriptionProvider.get();
  if (descriptionProvider == nullptr) {
    return;
  }

  for (const auto &element : elements) {
    pegium::utils::throw_if_cancelled(cancelToken);
    if (!element) {
      continue;
    }

    if (const auto *package =
            dynamic_cast<const PackageDeclaration *>(element.get())) {
      const auto nestedQualifier =
          _qualifiedNameProvider != nullptr
              ? _qualifiedNameProvider->getQualifiedName(qualifier, package->name)
              : package->name;
      collectExportedSymbols(package->elements, nestedQualifier, document, symbols,
                             cancelToken);
      continue;
    }

    const auto *type = dynamic_cast<const Type *>(element.get());
    if (type == nullptr) {
      continue;
    }

    auto name = type_name(*type);
    if (name.empty()) {
      continue;
    }
    if (!qualifier.empty() && _qualifiedNameProvider != nullptr) {
      name = _qualifiedNameProvider->getQualifiedName(qualifier, name);
    }
    if (auto description =
            descriptionProvider->createDescription(*type, document, name);
        description.has_value()) {
      symbols.push_back(std::move(*description));
    }
  }
}

pegium::workspace::LocalSymbols
DomainModelScopeComputation::collectLocalSymbols(
    const pegium::workspace::Document &document,
    const pegium::utils::CancellationToken &cancelToken) const {
  pegium::workspace::LocalSymbols symbols;
  const auto *model =
      pegium::ast_ptr_cast<DomainModel>(document.parseResult.value);
  if (model == nullptr) {
    return symbols;
  }

  (void)processContainer(*model, document, symbols, cancelToken);
  return symbols;
}

std::vector<pegium::workspace::AstNodeDescription>
DomainModelScopeComputation::processContainer(
    const DomainModel &container, const pegium::workspace::Document &document,
    pegium::workspace::LocalSymbols &symbols,
    const pegium::utils::CancellationToken &cancelToken) const {
  std::vector<pegium::workspace::AstNodeDescription> localDescriptions;
  const auto *descriptionProvider =
      coreServices.workspace.astNodeDescriptionProvider.get();
  if (descriptionProvider == nullptr) {
    return localDescriptions;
  }

  for (const auto &element : elements_of(container)) {
    pegium::utils::throw_if_cancelled(cancelToken);
    if (!element) {
      continue;
    }

    if (const auto *type = dynamic_cast<const Type *>(element.get())) {
      auto name = type_name(*type);
      if (name.empty()) {
        continue;
      }
      if (auto description =
              descriptionProvider->createDescription(*type, document, name);
          description.has_value()) {
        localDescriptions.push_back(std::move(*description));
      }
      continue;
    }

    const auto *package = dynamic_cast<const PackageDeclaration *>(element.get());
    if (package == nullptr) {
      continue;
    }

    auto nestedDescriptions =
        processContainer(*package, document, symbols, cancelToken);
    for (const auto &nested : nestedDescriptions) {
      if (nested.node == nullptr) {
        continue;
      }
      auto qualifiedName =
          _qualifiedNameProvider != nullptr
              ? _qualifiedNameProvider->getQualifiedName(*package, nested.name)
              : package->name + "." + nested.name;
      if (auto description = descriptionProvider->createDescription(
              *nested.node, document, std::move(qualifiedName));
          description.has_value()) {
        localDescriptions.push_back(std::move(*description));
      }
    }
  }

  for (const auto &description : localDescriptions) {
    symbols.emplace(&container, description);
  }
  return localDescriptions;
}

std::vector<pegium::workspace::AstNodeDescription>
DomainModelScopeComputation::processContainer(
    const PackageDeclaration &container,
    const pegium::workspace::Document &document,
    pegium::workspace::LocalSymbols &symbols,
    const pegium::utils::CancellationToken &cancelToken) const {
  std::vector<pegium::workspace::AstNodeDescription> localDescriptions;
  const auto *descriptionProvider =
      coreServices.workspace.astNodeDescriptionProvider.get();
  if (descriptionProvider == nullptr) {
    return localDescriptions;
  }

  for (const auto &element : elements_of(container)) {
    pegium::utils::throw_if_cancelled(cancelToken);
    if (!element) {
      continue;
    }

    if (const auto *type = dynamic_cast<const Type *>(element.get())) {
      auto name = type_name(*type);
      if (name.empty()) {
        continue;
      }
      if (auto description =
              descriptionProvider->createDescription(*type, document, name);
          description.has_value()) {
        localDescriptions.push_back(std::move(*description));
      }
      continue;
    }

    const auto *package = dynamic_cast<const PackageDeclaration *>(element.get());
    if (package == nullptr) {
      continue;
    }

    auto nestedDescriptions =
        processContainer(*package, document, symbols, cancelToken);
    for (const auto &nested : nestedDescriptions) {
      if (nested.node == nullptr) {
        continue;
      }
      auto qualifiedName =
          _qualifiedNameProvider != nullptr
              ? _qualifiedNameProvider->getQualifiedName(*package, nested.name)
              : package->name + "." + nested.name;
      if (auto description = descriptionProvider->createDescription(
              *nested.node, document, std::move(qualifiedName));
          description.has_value()) {
        localDescriptions.push_back(std::move(*description));
      }
    }
  }

  for (const auto &description : localDescriptions) {
    symbols.emplace(&container, description);
  }
  return localDescriptions;
}

} // namespace domainmodel::services::references
