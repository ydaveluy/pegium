#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides semantic tokens and the capability metadata they require.
class SemanticTokenProvider {
public:
  using StringIndexMap = utils::TransparentStringMap<std::uint32_t>;

  virtual ~SemanticTokenProvider() noexcept = default;

  /// Returns the supported token types indexed by their semantic-token id.
  [[nodiscard]] virtual StringIndexMap tokenTypes() const {
    return {};
  }

  /// Returns the supported token modifiers indexed by their semantic-token id.
  [[nodiscard]] virtual StringIndexMap tokenModifiers() const {
    return {};
  }

  /// Returns the semantic-token capability description to advertise.
  [[nodiscard]] virtual ::lsp::SemanticTokensOptions
  semanticTokensOptions() const = 0;

  /// Returns full-document semantic tokens.
  virtual std::optional<::lsp::SemanticTokens>
  semanticHighlight(
      const workspace::Document &document,
      const ::lsp::SemanticTokensParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  /// Returns semantic tokens restricted to a range.
  virtual std::optional<::lsp::SemanticTokens>
  semanticHighlightRange(
      const workspace::Document &document,
      const ::lsp::SemanticTokensRangeParams &params,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) const = 0;

  /// Returns a delta from a previously issued semantic-token result when supported.
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

} // namespace pegium
