#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides call hierarchy items and incoming/outgoing edges.
class CallHierarchyProvider {
public:
  virtual ~CallHierarchyProvider() noexcept = default;
  /// Returns the hierarchy roots at `params`.
  virtual std::vector<::lsp::CallHierarchyItem>
  prepareCallHierarchy(
      const workspace::Document &document,
      const ::lsp::CallHierarchyPrepareParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  /// Returns incoming call edges for the selected hierarchy item.
  ///
  /// `params.item` must come from `prepareCallHierarchy(...)` or otherwise
  /// refer to a managed workspace document owned by this server instance.
  virtual std::vector<::lsp::CallHierarchyIncomingCall>
  incomingCalls(
      const ::lsp::CallHierarchyIncomingCallsParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  /// Returns outgoing call edges for the selected hierarchy item.
  ///
  /// `params.item` must come from `prepareCallHierarchy(...)` or otherwise
  /// refer to a managed workspace document owned by this server instance.
  virtual std::vector<::lsp::CallHierarchyOutgoingCall>
  outgoingCalls(
      const ::lsp::CallHierarchyOutgoingCallsParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;
};

} // namespace pegium
