#pragma once

#include <cstddef>
#include <concepts>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <lsp/types.h>

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/syntax-tree/AstFeatures.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>

namespace pegium {

/// Describes where a formatting action applies relative to a selected CST node.
enum class FormattingMode : std::uint8_t { Prepend, Append };

/// Options that influence how a formatting action is resolved.
struct FormattingActionOptions {
  /// Higher priority wins when multiple actions target the same whitespace range.
  std::int32_t priority = 0;
  /// Keep at least the existing amount of whitespace.
  bool allowMore = false;
  /// Keep at most the existing amount of whitespace.
  bool allowLess = false;
};

/// Low-level whitespace instruction consumed by the formatter engine.
///
/// A move may describe:
/// - `characters`: inline spacing in spaces.
/// - `lines`: a line break count, followed by the current indentation.
/// - `tabs`: an indentation delta in indentation units.
///
/// Most formatters should use the helpers in `pegium::Formatting` instead
/// of constructing `FormattingMove` directly.
struct FormattingMove {
  std::optional<std::int32_t> characters;
  std::optional<std::int32_t> tabs;
  std::optional<std::int32_t> lines;
};

/// A formatting rule attached to one whitespace boundary.
///
/// A rule may contain several candidate moves. The engine picks the move that
/// best fits the current whitespace shape.
struct FormattingAction {
  FormattingActionOptions options;
  std::vector<FormattingMove> moves;
};

/// Callback used by the declarative DSL to collect formatting actions.
///
/// This is primarily an implementation detail of the formatting engine, but it
/// is also useful in tests that need to observe which regions are selected.
using FormattingCollector =
    std::function<void(const CstNodeView &, FormattingMode, const FormattingAction &)>;

/// Callback used by hidden-node formatters to replace the text of a comment or trivia node.
using HiddenTextCollector = std::function<void(const CstNodeView &, std::string)>;

class HiddenNodeFormatter;

/// Options used by `AbstractFormatter::formatMultilineComment(...)`.
///
/// The formatter is designed for Javadoc-style comments by default:
/// `/** ... */`, line prefix ` *`, and special handling for tag lines such as
/// `@param name description`.
struct MultilineCommentFormatOptions {
  /// Opening token that must appear at the start of the comment text.
  std::string start = "/**";
  /// Closing token that must appear at the end of the comment text.
  std::string end = "*/";
  /// Prefix inserted at the start of each intermediate line.
  ///
  /// Non-empty content automatically gets one separating space after this
  /// prefix when the prefix itself does not already end with whitespace.
  std::string newLineStart = " *";
  /// Extra prefix appended after `newLineStart` for tag continuation lines.
  std::string tagContinuation = "  ";
  /// Prefix that identifies tag lines such as `@param name description`.
  ///
  /// When omitted, tag normalization and tag continuation formatting are
  /// disabled.
  std::optional<std::string> tagStart = "@";
  /// Collapses runs of spaces and tabs inside content lines.
  bool normalizeWhitespace = true;
  /// Formats single-line comments into the multiline layout as well.
  bool forceMultiline = false;
  /// Maximum number of consecutive blank content lines to keep.
  std::size_t maxBlankLines = 1;
};

/// A selection of CST nodes to which whitespace rules can be attached.
///
/// Regions are usually obtained from `NodeFormatter`, for example with
/// `property`, `properties`, `keyword`, `keywords`, `node`, `nodes`, `cst`, or
/// `interior`.
class FormattingRegion {
public:
  FormattingRegion() = default;
  explicit FormattingRegion(std::vector<CstNodeView> nodes,
                            FormattingCollector collector = {})
      : _nodes(std::move(nodes)), _collector(std::move(collector)) {}

  /// Returns `true` when the selection is empty.
  [[nodiscard]] bool empty() const noexcept { return _nodes.empty(); }
  /// Returns the selected CST nodes in document order.
  [[nodiscard]] std::span<const CstNodeView> nodes() const noexcept { return _nodes; }

  /// Applies `formatting` to the whitespace before each selected node.
  FormattingRegion &prepend(const FormattingAction &formatting);
  /// Applies `formatting` to the whitespace after each selected node.
  FormattingRegion &append(const FormattingAction &formatting);
  /// Applies `formatting` both before and after each selected node.
  FormattingRegion &surround(const FormattingAction &formatting);

  /// Returns a sub-selection starting at `start`.
  ///
  /// Negative indices count from the end of the region.
  [[nodiscard]] FormattingRegion slice(std::ptrdiff_t start) const;
  /// Returns a sub-selection in the half-open range `[start, end)`.
  ///
  /// Negative indices count from the end of the region.
  [[nodiscard]] FormattingRegion slice(std::ptrdiff_t start,
                                       std::ptrdiff_t end) const;

private:
  [[nodiscard]] std::size_t normalizeIndex(std::ptrdiff_t index) const noexcept;

  std::vector<CstNodeView> _nodes;
  FormattingCollector _collector;
};

/// Formatter facade for one hidden CST node such as a comment.
///
/// Hidden nodes are CST-only. They can be targeted globally with
/// `AbstractFormatter::onHidden(...)` or selected inside one AST subtree with
/// `NodeFormatter::hidden(...)` / `NodeFormatter::hiddens(...)`.
class HiddenNodeFormatter {
public:
  HiddenNodeFormatter() = default;
  HiddenNodeFormatter(CstNodeView node, std::string ruleName,
                      std::int32_t baseIndentation,
                      ::lsp::FormattingOptions options,
                      FormattingCollector collector = {},
                      HiddenTextCollector textCollector = {})
      : _node(std::move(node)), _ruleName(std::move(ruleName)),
        _baseIndentation(baseIndentation), _options(std::move(options)),
        _collector(std::move(collector)),
        _textCollector(std::move(textCollector)) {}

  /// Returns the hidden CST node currently being formatted.
  [[nodiscard]] const CstNodeView &node() const noexcept { return _node; }

  /// Returns the terminal rule name that produced this hidden node.
  [[nodiscard]] std::string_view ruleName() const noexcept { return _ruleName; }

  /// Returns the current hidden node text before replacement.
  [[nodiscard]] std::string_view text() const noexcept { return _node.getText(); }

  /// Returns the current indentation level in indentation units.
  [[nodiscard]] std::int32_t baseIndentation() const noexcept {
    return _baseIndentation;
  }

  /// Returns the whitespace prefix that corresponds to `baseIndentation() + delta`.
  [[nodiscard]] std::string indentation(std::int32_t delta = 0) const;

  /// Replaces the full hidden node text.
  void replace(std::string text);

  /// Applies `formatting` to the whitespace before this hidden node.
  HiddenNodeFormatter &prepend(const FormattingAction &formatting);
  /// Applies `formatting` to the whitespace after this hidden node.
  HiddenNodeFormatter &append(const FormattingAction &formatting);
  /// Applies `formatting` both before and after this hidden node.
  HiddenNodeFormatter &surround(const FormattingAction &formatting);

private:
  CstNodeView _node;
  std::string _ruleName;
  std::int32_t _baseIndentation = 0;
  ::lsp::FormattingOptions _options;
  FormattingCollector _collector;
  HiddenTextCollector _textCollector;
};

template <typename NodeT> class NodeFormatter;

/// Entry point used by `AbstractFormatter::format` to obtain scoped formatters.
class FormattingBuilder {
public:
  explicit FormattingBuilder(FormattingCollector collector = {})
      : _collector(std::move(collector)) {}

  /// Returns a formatter scoped to `node`.
  ///
  /// The returned `NodeFormatter` resolves features and keywords relative to the
  /// CST subtree owned by `node`.
  template <typename NodeT>
    requires std::derived_from<std::remove_cv_t<NodeT>, AstNode>
  [[nodiscard]] NodeFormatter<std::remove_cv_t<NodeT>>
  getNodeFormatter(const NodeT *node) const {
    return NodeFormatter<std::remove_cv_t<NodeT>>(node, _collector);
  }

private:
  FormattingCollector _collector;
};

/// Declarative selector API scoped to a single AST node.
///
/// Typical usage:
/// ```cpp
/// auto formatter = builder.getNodeFormatter(entity);
/// formatter.keyword("entity").append(oneSpace);
/// auto openBrace = formatter.keyword("{");
/// auto closeBrace = formatter.keyword("}");
/// openBrace.prepend(oneSpace).append(newLine);
/// closeBrace.prepend(newLine);
/// formatter.interior(openBrace, closeBrace).prepend(indent);
/// ```
///
/// Variables such as `openBrace` or `closeBrace` are regular
/// `FormattingRegion` objects. There is no dedicated "open region" type.
template <typename NodeT> class NodeFormatter {
public:
  explicit NodeFormatter(const NodeT *node, FormattingCollector collector)
      : _node(node), _collector(std::move(collector)) {}

  /// Selects the CST node mapped to `Feature`.
  ///
  /// For single-valued features this targets the corresponding CST node.
  /// For vector features this returns the first matching CST node.
  template <auto Feature>
    requires pegium::detail::AstNodeFeature<NodeT, Feature>
  [[nodiscard]] FormattingRegion property() const {
    if (_node == nullptr) {
      return FormattingRegion({}, _collector);
    }
    if (const auto node = find_node_for_feature<Feature>(*_node); node.has_value()) {
      return FormattingRegion({*node}, _collector);
    }
    return FormattingRegion({}, _collector);
  }

  /// Selects the `index`-th CST node mapped to a vector feature.
  template <auto Feature>
    requires pegium::detail::VectorAstNodeFeature<NodeT, Feature>
  [[nodiscard]] FormattingRegion property(std::size_t index) const {
    if (_node == nullptr) {
      return FormattingRegion({}, _collector);
    }
    if (const auto node = find_node_for_feature<Feature>(*_node, index);
        node.has_value()) {
      return FormattingRegion({*node}, _collector);
    }
    return FormattingRegion({}, _collector);
  }

  /// Selects all CST nodes mapped to the given feature list.
  ///
  /// This is the plural form of `property` and is the preferred selector for
  /// vector features such as child element lists.
  template <auto... Features>
    requires(sizeof...(Features) > 0 &&
             (pegium::detail::AstNodeFeature<NodeT, Features> && ...))
  [[nodiscard]] FormattingRegion properties() const {
    std::vector<CstNodeView> regions;
    if (_node == nullptr) {
      return FormattingRegion(std::move(regions), _collector);
    }
    (appendFeatureNodes<Features>(regions), ...);
    return FormattingRegion(std::move(regions), _collector);
  }

  /// Selects the `index`-th occurrence of `keyword` in the current node scope.
  [[nodiscard]] FormattingRegion keyword(std::string_view keyword,
                                         std::size_t index = 0) const {
    if (_node == nullptr || !_node->hasCstNode()) {
      return FormattingRegion({}, _collector);
    }
    if (const auto node =
            find_node_for_keyword(_node->getCstNode(), keyword, index);
        node.has_value()) {
      return FormattingRegion({*node}, _collector);
    }
    return FormattingRegion({}, _collector);
  }

  /// Selects all occurrences of each given keyword in the current node scope.
  template <typename... Keywords>
    requires(sizeof...(Keywords) > 0 &&
             (std::convertible_to<Keywords, std::string_view> && ...))
  [[nodiscard]] FormattingRegion keywords(Keywords &&...keywords) const {
    std::vector<CstNodeView> regions;
    if (_node != nullptr && _node->hasCstNode()) {
      (appendKeywordNodes(regions,
                          std::string_view(std::forward<Keywords>(keywords))),
       ...);
    }
    return FormattingRegion(std::move(regions), _collector);
  }

  /// Selects the `index`-th hidden node produced by terminal rule `ruleName`.
  ///
  /// The search spans the full CST subtree of the current AST node.
  [[nodiscard]] FormattingRegion hidden(std::string_view ruleName,
                                        std::size_t index = 0) const {
    if (_node == nullptr || !_node->hasCstNode()) {
      return FormattingRegion({}, _collector);
    }
    const auto nodes = findHiddenNodes(ruleName);
    if (index < nodes.size()) {
      return FormattingRegion({nodes[index]}, _collector);
    }
    return FormattingRegion({}, _collector);
  }

  /// Selects all hidden nodes produced by terminal rule `ruleName`.
  ///
  /// The search spans the full CST subtree of the current AST node.
  [[nodiscard]] FormattingRegion hiddens(std::string_view ruleName) const {
    if (_node == nullptr || !_node->hasCstNode()) {
      return FormattingRegion({}, _collector);
    }
    return FormattingRegion(findHiddenNodes(ruleName), _collector);
  }

  /// Selects the CST nodes strictly between `start` and `end`.
  ///
  /// Both inputs must contain exactly one node. This is typically used with two
  /// named regions such as an opening and a closing brace.
  [[nodiscard]] FormattingRegion interior(const FormattingRegion &start,
                                          const FormattingRegion &end) const {
    if (start.nodes().size() != 1 || end.nodes().size() != 1) {
      return FormattingRegion({}, _collector);
    }
    return FormattingRegion(get_interior_nodes(start.nodes()[0], end.nodes()[0]),
                            _collector);
  }

private:
  template <auto Feature> void appendFeatureNodes(std::vector<CstNodeView> &out) const {
    const auto nodes = find_nodes_for_feature<Feature>(*_node);
    out.insert(out.end(), nodes.begin(), nodes.end());
  }

  void appendKeywordNodes(std::vector<CstNodeView> &out,
                          std::string_view keyword) const {
    const auto nodes = find_nodes_for_keyword(_node->getCstNode(), keyword);
    out.insert(out.end(), nodes.begin(), nodes.end());
  }

  [[nodiscard]] std::vector<CstNodeView>
  findHiddenNodes(std::string_view ruleName) const {
    std::vector<CstNodeView> nodes;
    const auto root = _node->getCstNode();
    for (auto candidate = root.next();
         candidate.valid() && candidate.getBegin() < root.getEnd();
         candidate = candidate.next()) {
      if (!candidate.isHidden()) {
        continue;
      }
      const auto terminalRuleName = get_terminal_rule_name(candidate);
      if (terminalRuleName.has_value() && *terminalRuleName == ruleName) {
        nodes.push_back(candidate);
      }
    }
    return nodes;
  }

  const NodeT *_node;
  FormattingCollector _collector;
};

/// Base class for declarative formatters.
///
/// Typical implementations register one callback per concrete AST node type in
/// their constructor by calling `on<T>(...)`. The engine handles AST traversal,
/// CST whitespace edit generation, range filtering, and comment preservation.
///
/// Advanced implementations may still override `format(...)` directly. The
/// default implementation dispatches the callbacks registered with `on<T>(...)`
/// using exact runtime type matching.
class AbstractFormatter : protected DefaultLanguageService,
                          public ::pegium::Formatter {
public:
  using DefaultLanguageService::DefaultLanguageService;

  /// Formats the full document when the parse result is error-free.
  std::vector<::lsp::TextEdit>
  formatDocument(const workspace::Document &document,
                 const ::lsp::DocumentFormattingParams &params,
                 const utils::CancellationToken &cancelToken) const override;

  /// Formats a document range when it is safe to do so.
  std::vector<::lsp::TextEdit>
  formatDocumentRange(const workspace::Document &document,
                      const ::lsp::DocumentRangeFormattingParams &params,
                      const utils::CancellationToken &cancelToken) const override;

  /// Formats the current line on type, reusing the same declarative rules.
  std::vector<::lsp::TextEdit>
  formatDocumentOnType(const workspace::Document &document,
                       const ::lsp::DocumentOnTypeFormattingParams &params,
                       const utils::CancellationToken &cancelToken) const override;

  /// Returns the trigger configuration for on-type formatting, if any.
  [[nodiscard]] std::optional<::lsp::DocumentOnTypeFormattingOptions>
  formatOnTypeOptions() const noexcept override;

protected:
  struct SpaceActionBuilder {
    std::int32_t count = 0;

    [[nodiscard]] FormattingAction
    operator()(FormattingActionOptions options = {}) const {
      return FormattingAction{
          .options = std::move(options),
          .moves = {FormattingMove{.characters = count}},
      };
    }

    [[nodiscard]] operator FormattingAction() const { return operator()({}); }
  };

  struct LineActionBuilder {
    std::int32_t count = 0;

    [[nodiscard]] FormattingAction
    operator()(FormattingActionOptions options = {}) const {
      return FormattingAction{
          .options = std::move(options),
          .moves = {FormattingMove{.lines = count}},
      };
    }

    [[nodiscard]] operator FormattingAction() const { return operator()({}); }
  };

  struct IndentActionBuilder {
    [[nodiscard]] FormattingAction
    operator()(FormattingActionOptions options = {}) const {
      return FormattingAction{
          .options = std::move(options),
          .moves = {FormattingMove{.tabs = 1, .lines = 1}},
      };
    }

    [[nodiscard]] operator FormattingAction() const { return operator()({}); }
  };

  struct NoIndentActionBuilder {
    [[nodiscard]] FormattingAction
    operator()(FormattingActionOptions options = {}) const {
      return FormattingAction{
          .options = std::move(options),
          .moves = {FormattingMove{.tabs = 0}},
      };
    }

    [[nodiscard]] operator FormattingAction() const { return operator()({}); }
  };

  using MultilineCommentFormatOptions = pegium::MultilineCommentFormatOptions;
  using HiddenNodeFormatter = pegium::HiddenNodeFormatter;

  struct BlockFormatOptions {
    std::optional<FormattingAction> beforeOpen;
    std::optional<FormattingAction> insideEmpty;
    std::optional<FormattingAction> afterOpen;
    std::optional<FormattingAction> beforeClose;
    std::optional<FormattingAction> contentIndent;
  };

  struct SeparatedListFormatOptions {
    std::optional<FormattingAction> beforeSeparator;
    std::optional<FormattingAction> afterSeparator;
  };

  struct LineCommentFormatOptions {
    std::string start = "//";
    bool normalizeWhitespace = true;
    bool ensureSpaceAfterStart = true;
  };

  inline static constexpr SpaceActionBuilder noSpace{0};
  inline static constexpr SpaceActionBuilder oneSpace{1};
  inline static constexpr LineActionBuilder newLine{1};
  inline static constexpr IndentActionBuilder indent{};
  inline static constexpr NoIndentActionBuilder noIndent{};

  template <typename... Actions>
    requires(sizeof...(Actions) > 0 &&
             (std::convertible_to<Actions, FormattingAction> && ...))
  [[nodiscard]] static FormattingAction fit(Actions &&...actions) {
    return fitActions(
        {static_cast<FormattingAction>(std::forward<Actions>(actions))...});
  }

  [[nodiscard]] static FormattingAction
  spaces(std::int32_t count, FormattingActionOptions options = {});

  [[nodiscard]] static FormattingAction
  newLines(std::int32_t count, FormattingActionOptions options = {});

  [[nodiscard]] static std::string
  formatMultilineComment(std::string_view text,
                         const MultilineCommentFormatOptions &options = {});

  [[nodiscard]] static std::string
  formatMultilineComment(const HiddenNodeFormatter &comment) {
    return formatMultilineComment(comment, MultilineCommentFormatOptions{});
  }

  [[nodiscard]] static std::string
  formatMultilineComment(const HiddenNodeFormatter &comment,
                         const MultilineCommentFormatOptions &options);

  static void formatBlock(FormattingRegion open, FormattingRegion close,
                          FormattingRegion content = {}) {
    formatBlock(std::move(open), std::move(close), std::move(content),
                BlockFormatOptions{});
  }

  static void formatBlock(FormattingRegion open, FormattingRegion close,
                          FormattingRegion content,
                          BlockFormatOptions options);

  static void formatSeparatedList(FormattingRegion separators) {
    formatSeparatedList(std::move(separators), SeparatedListFormatOptions{});
  }

  static void formatSeparatedList(FormattingRegion separators,
                                  SeparatedListFormatOptions options);

  [[nodiscard]] static std::string
  formatLineComment(std::string_view text) {
    return formatLineComment(text, LineCommentFormatOptions{});
  }

  [[nodiscard]] static std::string
  formatLineComment(std::string_view text,
                    LineCommentFormatOptions options);

  [[nodiscard]] static std::string
  formatLineComment(const HiddenNodeFormatter &comment) {
    return formatLineComment(comment, LineCommentFormatOptions{});
  }

  [[nodiscard]] static std::string
  formatLineComment(const HiddenNodeFormatter &comment,
                    LineCommentFormatOptions options);

  /// Registers a formatter method of the current formatter object for `NodeT`.
  ///
  /// This convenience overload binds `this` automatically, so implementations
  /// can write `on<MyNode>(&MyFormatter::formatMyNode)`.
  template <typename NodeT, typename FormatterT>
    requires std::derived_from<std::remove_cv_t<NodeT>, AstNode> &&
             std::derived_from<std::remove_cv_t<FormatterT>, AbstractFormatter>
  void on(void (FormatterT::*method)(FormattingBuilder &,
                                     const std::remove_cv_t<NodeT> *) const) {
    if (method == nullptr) {
      return;
    }
    using StoredNode = std::remove_cv_t<NodeT>;
    registerFormatter<StoredNode>(
        [this, method](FormattingBuilder &builder, const StoredNode *node) {
          (static_cast<const FormatterT *>(this)->*method)(builder, node);
        });
  }

  /// Registers a hidden-node formatter method of the current formatter object.
  template <typename FormatterT>
    requires std::derived_from<std::remove_cv_t<FormatterT>, AbstractFormatter>
  void onHidden(std::string_view terminalRuleName,
                void (FormatterT::*method)(HiddenNodeFormatter &) const) {
    if (method == nullptr) {
      return;
    }
    registerHiddenFormatter(terminalRuleName,
                            [this, method](HiddenNodeFormatter &hidden) {
                              (static_cast<const FormatterT *>(this)->*method)(
                                  hidden);
                            });
  }

  /// Returns `true` when the requested range can be formatted safely.
  [[nodiscard]] bool
  isFormatRangeErrorFree(const workspace::Document &document,
                         const ::lsp::Range &range) const;

  /// Runs the formatting engine and materializes LSP text edits.
  [[nodiscard]] std::vector<::lsp::TextEdit>
  doDocumentFormat(const workspace::Document &document,
                   const ::lsp::FormattingOptions &options,
                   const std::optional<::lsp::Range> &range,
                   const utils::CancellationToken &cancelToken) const;

  /// Declares formatting rules for one AST node.
  ///
  /// The default implementation dispatches the callback previously registered
  /// with `on<T>(...)` for the exact runtime type of `node`. Override this only
  /// for advanced cases that need custom dispatch or fallback behavior.
  virtual void format(FormattingBuilder &builder, const AstNode *node) const;

  /// Declares formatting rules for one hidden CST node such as a comment.
  ///
  /// The default implementation dispatches the callback previously registered
  /// with `onHidden(...)` for the node's terminal rule name.
  virtual void formatHidden(HiddenNodeFormatter &hidden) const;

private:
  template <typename NodeT>
  using TypedFormattingCallback =
      std::function<void(FormattingBuilder &, const NodeT *)>;
  using HiddenNodeCallback = std::function<void(HiddenNodeFormatter &)>;
  using UntypedFormattingCallback =
      std::function<void(FormattingBuilder &, const AstNode *)>;

  [[nodiscard]] static FormattingAction
  fitActions(std::initializer_list<FormattingAction> actions);

  template <typename NodeT>
  void registerFormatter(TypedFormattingCallback<NodeT> callback) {
    if (!callback) {
      return;
    }

    const auto nodeType = std::type_index(typeid(NodeT));
    _formatters.insert_or_assign(
        nodeType,
        [callback = std::move(callback)](FormattingBuilder &builder,
                                         const AstNode *node) {
          callback(builder, static_cast<const NodeT *>(node));
        });
  }

  void registerHiddenFormatter(std::string_view terminalRuleName,
                               HiddenNodeCallback callback);

  std::unordered_map<std::type_index, UntypedFormattingCallback> _formatters;
  utils::TransparentStringMap<HiddenNodeCallback> _hiddenFormatters;
};

} // namespace pegium
