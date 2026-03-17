#pragma once

#include <lsp/types.h>

#include <pegium/lsp/LanguageServer.hpp>
#include <pegium/services/DefaultSharedLspService.hpp>
#include <pegium/utils/Event.hpp>

namespace pegium::lsp {

class DefaultLanguageServer : public LanguageServer,
                              protected services::DefaultSharedLspService {
public:
  using services::DefaultSharedLspService::DefaultSharedLspService;
  ~DefaultLanguageServer() noexcept override = default;

  [[nodiscard]] ::lsp::InitializeResult
  initialize(const ::lsp::InitializeParams &params) override;
  void initialized(const ::lsp::InitializedParams &params) override;
  utils::ScopedDisposable
  onInitialize(std::function<void(const ::lsp::InitializeParams &)> callback)
      override;
  utils::ScopedDisposable
  onInitialized(std::function<void(const ::lsp::InitializedParams &)> callback)
      override;

private:
  utils::EventEmitter<::lsp::InitializeParams> _onInitialize;
  utils::EventEmitter<::lsp::InitializedParams> _onInitialized;
};

} // namespace pegium::lsp
