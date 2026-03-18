#pragma once

#include <functional>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/utils/Disposable.hpp>

namespace pegium::lsp {

struct SemanticTokenInfo {
  ::lsp::Range range;
  std::string type;
  std::vector<std::string> modifiers;
};

using SemanticTokenAcceptor =
    std::function<void(SemanticTokenInfo token)>;

class AbstractSemanticTokenProvider : protected services::DefaultLanguageService,
                                public services::SemanticTokenProvider {
public:
  explicit AbstractSemanticTokenProvider(
      const services::Services &services);

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
  utils::ScopedDisposable _textDocumentCloseSubscription;
};

} // namespace pegium::lsp
