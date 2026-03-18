#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/TransparentStringHash.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class SemanticTokenProvider {
public:
  using StringIndexMap = utils::TransparentStringMap<std::uint32_t>;

  virtual ~SemanticTokenProvider() noexcept = default;

  [[nodiscard]] virtual StringIndexMap tokenTypes() const {
    return {};
  }

  [[nodiscard]] virtual StringIndexMap tokenModifiers() const {
    return {};
  }

  [[nodiscard]] virtual ::lsp::SemanticTokensOptions
  semanticTokensOptions() const = 0;

  virtual std::optional<::lsp::SemanticTokens>
  semanticHighlight(
      const workspace::Document &document,
      const ::lsp::SemanticTokensParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  virtual std::optional<::lsp::SemanticTokens>
  semanticHighlightRange(
      const workspace::Document &document,
      const ::lsp::SemanticTokensRangeParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  virtual std::optional<::lsp::OneOf<::lsp::SemanticTokens,
                                     ::lsp::SemanticTokensDelta>>
  semanticHighlightDelta(
      const workspace::Document &document,
      const ::lsp::SemanticTokensDeltaParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const {
    (void)document;
    (void)params;
    (void)cancelToken;
    return std::nullopt;
  }
};

} // namespace pegium::services
