#pragma once

#include <pegium/core/utils/FunctionRef.hpp>
#include <pegium/core/syntax-tree/ReferenceInfo.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium::references {

/// Enumerates the scope visible from one reference site.
class ScopeProvider {
public:
  virtual ~ScopeProvider() noexcept = default;

  /// Resolves the first visible scope entry for `context.referenceText`.
  ///
  /// When `context.container` is non-null, it must belong to a managed
  /// workspace document.
  [[nodiscard]] virtual const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const = 0;

  /// Visits visible scope entries in lexical order.
  ///
  /// When `context.container` is non-null, it must belong to a managed
  /// workspace document.
  [[nodiscard]] virtual bool visitScopeEntries(
      const ReferenceInfo &context,
      utils::function_ref<bool(const workspace::AstNodeDescription &)> visitor)
      const = 0;
};

} // namespace pegium::references
