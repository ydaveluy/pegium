#pragma once

#include <memory>
#include <ranges>
#include <typeindex>

#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/references/Scope.hpp>
#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/Documents.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::references {

/// Reference lookup context used by linking and completion.
///
/// Unlike `ReferenceInfo`, this structure can be populated even when the
/// document does not yet contain a concrete `AbstractReference` instance at the
/// cursor position.
struct ScopeQueryContext {
  const AbstractReference *reference = nullptr;
  AstNode *container = nullptr;
  std::string_view property;
  std::optional<std::size_t> index;
  std::string_view referenceText;
  std::type_index referenceType = std::type_index(typeid(void));
  const grammar::AbstractRule *rule = nullptr;
  const grammar::Assignment *assignment = nullptr;
  bool multi = false;

  [[nodiscard]] bool accepts(const AstNode *node) const noexcept {
    if (reference != nullptr) {
      return reference->accepts(node);
    }
    return assignment != nullptr ? assignment->acceptsReferenceTarget(node)
                                 : false;
  }
};

[[nodiscard]] inline ScopeQueryContext
makeScopeQueryContext(const ReferenceInfo &context) noexcept {
  ScopeQueryContext query{
      .reference = context.reference,
      .container = context.container,
      .property = context.property,
      .index = context.index,
  };
  if (context.reference != nullptr) {
    query.referenceText = context.reference->getRefText();
    query.referenceType = context.reference->getReferenceType();
    query.multi = context.reference->isMulti();
  }
  return query;
}

class ScopeProvider {
public:
  virtual ~ScopeProvider() noexcept = default;

  virtual std::shared_ptr<const Scope>
  getScope(const ReferenceInfo &context) const = 0;

  virtual std::shared_ptr<const Scope>
  getScope(const ScopeQueryContext &context) const {
    if (context.reference != nullptr) {
      return getScope(ReferenceInfo{.reference = context.reference,
                                    .container = context.container,
                                    .property = context.property,
                                    .index = context.index});
    }
    return std::shared_ptr<const Scope>{};
  }

  [[nodiscard]] virtual const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const noexcept {
    if (context.reference == nullptr) {
      return nullptr;
    }
    const auto scope = getScope(context);
    return scope != nullptr ? scope->getElement(context.reference->getRefText())
                            : nullptr;
  }

  [[nodiscard]] virtual const workspace::AstNodeDescription *
  getScopeEntry(const ScopeQueryContext &context) const noexcept {
    if (context.referenceText.empty()) {
      return nullptr;
    }
    const auto scope = getScope(context);
    return scope != nullptr ? scope->getElement(context.referenceText) : nullptr;
  }

  [[nodiscard]] virtual utils::stream<const workspace::AstNodeDescription *>
  getScopeEntries(const ReferenceInfo &context) const {
    if (context.reference == nullptr) {
      return utils::make_stream<const workspace::AstNodeDescription *>(
          std::views::empty<const workspace::AstNodeDescription *>);
    }
    const auto scope = getScope(context);
    if (scope == nullptr) {
      return utils::make_stream<const workspace::AstNodeDescription *>(
          std::views::empty<const workspace::AstNodeDescription *>);
    }
    return scope->getElements(context.reference->getRefText());
  }

  [[nodiscard]] virtual utils::stream<const workspace::AstNodeDescription *>
  getScopeEntries(const ScopeQueryContext &context) const {
    const auto scope = getScope(context);
    if (scope == nullptr) {
      return utils::make_stream<const workspace::AstNodeDescription *>(
          std::views::empty<const workspace::AstNodeDescription *>);
    }
    if (context.referenceText.empty()) {
      return scope->getAllElements();
    }
    return scope->getElements(context.referenceText);
  }
};

} // namespace pegium::references
