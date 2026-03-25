#pragma once

#include <functional>
#include <initializer_list>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <atomic>
#include <unordered_map>
#include <vector>

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/utils/Disposable.hpp>

namespace pegium {

/// One semantic-token payload before LSP delta encoding.
struct SemanticTokenInfo {
  ::lsp::Range range;
  std::string type;
  std::vector<std::string> modifiers;
};

/// Callback used by semantic-token providers to emit one token at a time.
using SemanticTokenAcceptor =
    std::function<void(SemanticTokenInfo token)>;

/// Shared semantic-token provider with caching and delta support.
class AbstractSemanticTokenProvider : protected DefaultLanguageService,
                                public ::pegium::SemanticTokenProvider {
public:
  explicit AbstractSemanticTokenProvider(
      const pegium::Services &services);

  [[nodiscard]] StringIndexMap tokenTypes() const override;

  [[nodiscard]] StringIndexMap tokenModifiers() const override;

  [[nodiscard]] ::lsp::SemanticTokensOptions
  semanticTokensOptions() const override;

  std::optional<::lsp::SemanticTokens>
  semanticHighlight(const workspace::Document &document,
                    const ::lsp::SemanticTokensParams &params,
                    const utils::CancellationToken &cancelToken) const override;

  std::optional<::lsp::SemanticTokens>
  semanticHighlightRange(const workspace::Document &document,
                         const ::lsp::SemanticTokensRangeParams &params,
                         const utils::CancellationToken &cancelToken) const override;

  std::optional<::lsp::OneOf<::lsp::SemanticTokens, ::lsp::SemanticTokensDelta>>
  semanticHighlightDelta(const workspace::Document &document,
                         const ::lsp::SemanticTokensDeltaParams &params,
                         const utils::CancellationToken &cancelToken) const override;

protected:
  /// Emits semantic tokens for `node`.
  virtual void highlightElement(const AstNode &node,
                                const SemanticTokenAcceptor &acceptor) const = 0;

  void highlightRange(const ::lsp::Range &range, std::string_view type,
                      const SemanticTokenAcceptor &acceptor,
                      std::initializer_list<std::string_view> modifiers = {}) const;

  void highlightNode(const AstNode &node, std::string_view type,
                     const SemanticTokenAcceptor &acceptor,
                     std::initializer_list<std::string_view> modifiers = {}) const;

  void highlightProperty(const AstNode &node, std::string_view feature,
                         std::string_view type,
                         const SemanticTokenAcceptor &acceptor,
                         std::initializer_list<std::string_view> modifiers = {},
                         std::optional<std::size_t> index = std::nullopt) const;

  void highlightKeyword(const AstNode &node, std::string_view keyword,
                        std::string_view type,
                        const SemanticTokenAcceptor &acceptor,
                        std::initializer_list<std::string_view> modifiers = {},
                        std::optional<std::size_t> index = std::nullopt) const;

private:
  struct SemanticTokenCacheEntry {
    std::vector<std::uint32_t> data;
    std::string resultId;
  };

  [[nodiscard]] std::optional<::lsp::SemanticTokens>
  buildSemanticTokens(const workspace::Document &document,
                      const std::optional<::lsp::Range> &range,
                      const utils::CancellationToken &cancelToken) const;

  [[nodiscard]] std::string nextSemanticTokenResultId() const;
  [[nodiscard]] std::optional<SemanticTokenCacheEntry>
  getSemanticTokenCache(std::string_view uri) const;
  void setSemanticTokenCache(std::string_view uri,
                             const ::lsp::SemanticTokens &tokens) const;
  void clearSemanticTokenCache(std::string_view uri) const;

  mutable std::mutex _semanticTokenCacheMutex;
  mutable utils::TransparentStringMap<SemanticTokenCacheEntry> _semanticTokenCache;
  mutable std::uint64_t _nextSemanticTokenResultId = 1;
  // Keep this as a byte flag instead of atomic<bool> for guided FuzzTest
  // builds: it prevents Pegium from exporting another instrumented weak
  // atomic<bool>::load that could recurse through sanitizer coverage.
  std::atomic<std::uint8_t> _supportsMultilineTokens{0};
  utils::ScopedDisposable _languageServerInitializeSubscription;
  utils::ScopedDisposable _textDocumentCloseSubscription;
};

} // namespace pegium
