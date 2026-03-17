#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class CallHierarchyProvider {
public:
  virtual ~CallHierarchyProvider() noexcept = default;
  virtual std::vector<::lsp::CallHierarchyItem>
  prepareCallHierarchy(
      const workspace::Document &document,
      const ::lsp::CallHierarchyPrepareParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  virtual std::vector<::lsp::CallHierarchyIncomingCall>
  incomingCalls(
      const ::lsp::CallHierarchyIncomingCallsParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  virtual std::vector<::lsp::CallHierarchyOutgoingCall>
  outgoingCalls(
      const ::lsp::CallHierarchyOutgoingCallsParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
