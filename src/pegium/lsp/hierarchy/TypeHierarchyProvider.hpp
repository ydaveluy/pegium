#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides type hierarchy items and supertype/subtype expansion.
class TypeHierarchyProvider {
public:
  virtual ~TypeHierarchyProvider() noexcept = default;
  /// Returns the hierarchy roots at `params`.
  virtual std::vector<::lsp::TypeHierarchyItem>
  prepareTypeHierarchy(
      const workspace::Document &document,
      const ::lsp::TypeHierarchyPrepareParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  /// Returns supertypes for the selected hierarchy item.
  ///
  /// `params.item` must come from `prepareTypeHierarchy(...)` or otherwise
  /// refer to a managed workspace document owned by this server instance.
  virtual std::vector<::lsp::TypeHierarchyItem>
  supertypes(
      const ::lsp::TypeHierarchySupertypesParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  /// Returns subtypes for the selected hierarchy item.
  ///
  /// `params.item` must come from `prepareTypeHierarchy(...)` or otherwise
  /// refer to a managed workspace document owned by this server instance.
  virtual std::vector<::lsp::TypeHierarchyItem>
  subtypes(const ::lsp::TypeHierarchySubtypesParams &params,
           const utils::CancellationToken &cancelToken =
               utils::default_cancel_token) const = 0;
};

} // namespace pegium
