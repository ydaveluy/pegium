#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class TypeHierarchyProvider {
public:
  virtual ~TypeHierarchyProvider() noexcept = default;
  virtual std::vector<::lsp::TypeHierarchyItem>
  prepareTypeHierarchy(
      const workspace::Document &document,
      const ::lsp::TypeHierarchyPrepareParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  virtual std::vector<::lsp::TypeHierarchyItem>
  supertypes(
      const ::lsp::TypeHierarchySupertypesParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  virtual std::vector<::lsp::TypeHierarchyItem>
  subtypes(const ::lsp::TypeHierarchySubtypesParams &params,
           const utils::CancellationToken &cancelToken =
               utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
