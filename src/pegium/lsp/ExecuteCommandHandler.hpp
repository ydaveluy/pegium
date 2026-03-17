#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>

namespace pegium::lsp {

using ExecuteCommandFunction = std::function<
    std::optional<::lsp::LSPAny>(const ::lsp::LSPArray &arguments,
                                 const utils::CancellationToken &cancelToken)>;

using ExecuteCommandAcceptor =
    std::function<void(std::string name, ExecuteCommandFunction execute)>;

class ExecuteCommandHandler {
public:
  virtual ~ExecuteCommandHandler() noexcept = default;

  [[nodiscard]] virtual std::vector<std::string> commands() const = 0;

  [[nodiscard]] virtual std::optional<::lsp::LSPAny>
  executeCommand(std::string_view name, const ::lsp::LSPArray &arguments,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium::lsp
