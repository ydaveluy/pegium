#pragma once

#include <functional>

#include <lsp/types.h>

#include <pegium/utils/Disposable.hpp>


namespace pegium::lsp {

class LanguageServer {
public:
  virtual ~LanguageServer() noexcept = default;

  [[nodiscard]] virtual ::lsp::InitializeResult
  initialize(const ::lsp::InitializeParams &params) = 0;
  virtual void initialized(const ::lsp::InitializedParams &params) = 0;
  virtual utils::ScopedDisposable onInitialize(
      std::function<void(const ::lsp::InitializeParams &)> callback) = 0;
  virtual utils::ScopedDisposable onInitialized(
      std::function<void(const ::lsp::InitializedParams &)> callback) = 0;
};

} // namespace pegium::lsp
