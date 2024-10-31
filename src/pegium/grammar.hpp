#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <pegium/IParser.hpp>
#include <pegium/syntax-tree.hpp>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace pegium::grammar {

consteval auto make_tolower() {
  std::array<unsigned char, 256> lookup{};
  for (int c = 0; c < 256; ++c) {
    if (c >= 'A' && c <= 'Z') {
      lookup[c] = static_cast<unsigned char>(c) + ('a' - 'A');
    } else {
      lookup[c] = static_cast<unsigned char>(c);
    }
  }
  return lookup;
}
static constexpr auto tolower_array = make_tolower();

/// Fast helper function to convert a char to lower case
/// @param c the char to convert
/// @return the lower case char
constexpr char tolower(char c) {
  return static_cast<char>(tolower_array[static_cast<unsigned char>(c)]);
}

/// Build an array of char (remove the ending '\0')
/// @tparam N the number of char without the ending '\0'
template <std::size_t N> struct char_array_builder {
  std::array<char, N - 1> value;
  explicit(false) consteval char_array_builder(char const (&pp)[N]) {
    for (std::size_t i = 0; i < value.size(); ++i) {
      value[i] = pp[i];
    }
  }
};

static constexpr std::size_t PARSE_ERROR =
    std::numeric_limits<std::size_t>::max();
constexpr bool success(std::size_t len) { return len != PARSE_ERROR; }

constexpr bool fail(std::size_t len) { return len == PARSE_ERROR; }
class Context;
struct GrammarElement {
  constexpr virtual ~GrammarElement() noexcept = default;
  // parse the input text from a terminal: no hidden/ignored token between
  // elements
  virtual std::size_t parse_terminal(std::string_view sv) const noexcept = 0;
  // parse the input text from a rule: hidden/ignored token between elements are
  // skipped
  virtual std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                 Context &c) const noexcept = 0;
};

template <typename T>
concept isGrammarElement = std::derived_from<T, GrammarElement>;

struct Rule : GrammarElement {
  virtual ParseResult parse(std::string_view text, Context &c) const;
};
class Context final {
public:
  explicit constexpr Context(std::vector<const Rule *> &&hiddens)
      : _hiddens{std::move(hiddens)} {
    // TODO check dynamically that all rules are Terminal Rules
  }

  constexpr std::size_t skipHiddenNodes(std::string_view sv,
                                        CstNode &node) const noexcept {

    std::size_t i = 0;
    while (true) {
      bool matched = false;

      for (const auto *rule : _hiddens) {
        const auto len = rule->parse_terminal({sv.data() + i, sv.size() - i});
        if (success(len)) {
          assert(
              len &&
              "An hidden terminal rule must consume at least one character.");

          auto &hiddenNode = node.content.emplace_back();
          hiddenNode.text = {sv.data() + i, len};
          hiddenNode.grammarSource = rule;
          hiddenNode.isLeaf = true;
          hiddenNode.hidden = true;

          i += len;
          matched = true;
        }
      }

      if (!matched) {
        break;
      }
    }
    return i;
  }

private:
  std::vector<const Rule *> _hiddens;
};
using ContextProvider = std::function<Context()>;

inline ParseResult Rule::parse(std::string_view text, Context &c) const {
  ParseResult result;
  result.root_node = std::make_shared<RootCstNode>();
  result.root_node->fullText = text;
  std::string_view sv = result.root_node->fullText;
  result.root_node->text = result.root_node->fullText;
  result.root_node->grammarSource = this;

  auto i = c.skipHiddenNodes(sv, *result.root_node);

  result.len =
      i + parse_rule({sv.data() + i, sv.size() - i}, *result.root_node, c);

  result.ret = result.len == sv.size();

  return result;
}

struct ParserRule final : Rule {
  ParserRule() = default;
  ParserRule(const ParserRule &) = delete;
  ParserRule &operator=(const ParserRule &) = delete;
  std::size_t parse_terminal(std::string_view sv) const noexcept override {
    return _element->parse_terminal(sv);
  }
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const noexcept override {
    auto i = _element->parse_rule(sv, parent, c);
    if (fail(i)) {
      return PARSE_ERROR;
    }
    auto &node = parent.content.emplace_back();
    node.text = {sv.data(), i};
    node.grammarSource = this;
    node.isLeaf = true;

    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }

  /// Initialize the rule with a list of elements
  /// @tparam ...Args
  /// @param ...args the list of elements
  /// @return a reference to the rule
  // template <typename... Args>
  //  requires(std::derived_from<Args, GrammarElement> && ...)
  ParserRule &operator()(auto... args) {
    _element =
        std::make_shared<std::decay_t<decltype((args, ...))>>((args, ...));
    return *this;
  }

private:
  std::shared_ptr<GrammarElement> _element;
};
struct DataTypeRule final : Rule {
  DataTypeRule() = default;
  DataTypeRule(const DataTypeRule &) = delete;
  DataTypeRule &operator=(const DataTypeRule &) = delete;
  std::size_t parse_terminal(std::string_view sv) const noexcept override {
    return _element->parse_terminal(sv);
  }
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const noexcept override {
    auto i = _element->parse_rule(sv, parent, c);
    if (fail(i)) {
      return PARSE_ERROR;
    }
    auto &node = parent.content.emplace_back();
    node.text = {sv.data(), i};
    node.grammarSource = this;
    node.isLeaf = true;

    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }

  /// Initialize the rule with a list of elements
  /// @tparam ...Args
  /// @param ...args the list of elements
  /// @return a reference to the rule
  // template <typename... Args>
  // requires(std::derived_from<Args, GrammarElement> && ...)
  DataTypeRule &operator()(auto... args) {
    _element =
        std::make_shared<std::decay_t<decltype((args, ...))>>((args, ...));
    return *this;
  }

private:
  std::shared_ptr<GrammarElement> _element;
};

struct TerminalRule final : Rule {
  TerminalRule() = default;
  TerminalRule(const TerminalRule &) = delete;
  TerminalRule &operator=(const TerminalRule &) = delete;

  ParseResult parse(std::string_view text, Context &c) const override {
    ParseResult result;
    result.root_node = std::make_shared<RootCstNode>();
    result.root_node->fullText = text;
    std::string_view sv = result.root_node->fullText;
    result.root_node->grammarSource = this;

    result.len = parse_terminal(sv);
    result.root_node->isLeaf = true;

    if (fail(result.len))
      result.root_node->text = {};

    result.ret = result.len == sv.size();

    return result;
  }
  std::size_t parse_terminal(std::string_view sv) const noexcept override {
    return _element->parse_terminal(sv);
  }
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const noexcept override {
    auto i = parse_terminal(sv);
    if (fail(i)) {
      return PARSE_ERROR;
    }
    // Do not create a node if the rule is ignored
    if (_kind != TerminalRule::Kind::Ignored) {
      auto &node = parent.content.emplace_back();
      node.text = {sv.data(), i};
      node.grammarSource = this;
      node.isLeaf = true;
      node.hidden = _kind == TerminalRule::Kind::Hidden;
    }
    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }

  /// Initialize the rule with a list of elements
  /// @tparam ...Args
  /// @param ...args the list of elements
  /// @return a reference to the rule
  template <typename... Args>
  // requires(std::derived_from<std::unwrap_ref_decay_t<Args>, GrammarElement>
  // && ...)
  TerminalRule &operator()(Args... args) {
    _element =
        std::make_shared<std::decay_t<decltype((args, ...))>>((args, ...));
    return *this;
  }

  /// @return true if the rule is  hidden or ignored, false otherwise
  bool hidden() const noexcept { return _kind != Kind::Normal; }
  /// @return true if the rule is ignored, false otherwise
  bool ignored() const noexcept { return _kind == Kind::Ignored; }

  TerminalRule &hide() noexcept {
    _kind = Kind::Hidden;
    return *this;
  }
  TerminalRule &ignore() noexcept {
    _kind = Kind::Ignored;
    return *this;
  }

private:
  enum class Kind : std::uint8_t {
    // a terminal mapped to a normal CstNode (non-hidden)
    Normal,
    // a terminal mapped to an hidden CstNode
    Hidden,
    // a terminal not mapped to a CstNode
    Ignored
  };

  std::shared_ptr<GrammarElement> _element;
  Kind _kind = Kind::Normal;
};

/// Build an array of char (remove the ending '\0')
/// @tparam N the number of char without the ending '\0'
template <std::size_t N> struct range_array_builder {
  std::array<bool, 256> value{};
  explicit(false) constexpr range_array_builder(char const (&s)[N]) {
    std::size_t i = 0;
    while (i < N - 1) {
      if (i + 2 < N - 1 && s[i + 1] == '-') {
        for (auto c = s[i]; c <= s[i + 2]; ++c) {
          value[static_cast<unsigned char>(c)] = true;
        }
        i += 3;
      } else {
        value[static_cast<unsigned char>(s[i])] = true;
        i += 1;
      }
    }
  }
};
static constexpr const auto isword_lookup =
    range_array_builder{"a-zA-Z0-9_"}.value;
constexpr bool isword(char c) {
  return isword_lookup[static_cast<unsigned char>(c)];
}

template <std::array<bool, 256> lookup>
struct CharactersRanges final : GrammarElement {
  constexpr ~CharactersRanges() override = default;

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    auto i = CharactersRanges::parse_terminal(sv);
    if (fail(i) || (sv.size() > i && isword(sv[i - 1]) && isword(sv[i]))) {
      return PARSE_ERROR;
    }

    auto &node = parent.content.emplace_back();
    node.text = {sv.data(), i};
    node.grammarSource = this;
    node.isLeaf = true;

    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }
  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {

    return (!sv.empty() && lookup[static_cast<unsigned char>(sv[0])])
               ? 1
               : PARSE_ERROR;
  }
  /// Create an insensitive Characters Ranges
  /// @return the insensitive Characters Ranges
  constexpr auto i() const noexcept {
    return CharactersRanges<toInsensitive()>{};
  }
  /// Negate the Characters Ranges
  /// @return the negated Characters Ranges
  constexpr auto operator!() const noexcept {
    return CharactersRanges<toNegated()>{};
  }

private:
  static constexpr auto toInsensitive() {
    auto newLookup = lookup;
    for (char c = 'a'; c <= 'z'; ++c) {
      auto lower = static_cast<unsigned char>(c);
      auto upper = static_cast<unsigned char>(c - 'a' + 'A');

      newLookup[lower] |= lookup[upper];
      newLookup[upper] |= lookup[lower];
    }
    return newLookup;
  }
  static constexpr auto toNegated() {
    decltype(lookup) newLookup;
    std::ranges::transform(lookup, newLookup.begin(), std::logical_not{});
    return newLookup;
  }
};

/// Concat 2 Characters Ranges
/// @tparam otherLookup the other Characters Ranges
/// @param
/// @return the concatenation of both Characters Ranges
template <std::array<bool, 256> lhs, std::array<bool, 256> rhs>
constexpr auto operator|(CharactersRanges<lhs>,
                         CharactersRanges<rhs>) noexcept {
  auto toOr = [] {
    std::array<bool, 256> newLookup;
    std::ranges::transform(lhs, rhs, newLookup.begin(), std::logical_or{});
    return newLookup;
  };
  return CharactersRanges<toOr()>{};
}

template <range_array_builder builder> consteval auto operator""_cr() {
  return CharactersRanges<builder.value>{};
}

template <auto literal, bool case_sensitive = true>
struct Literal final : GrammarElement {
  constexpr ~Literal() override = default;

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {

    auto i = Literal::parse_terminal(sv);
    if (fail(i) || (isword(literal.back()) && isword(sv[i]))) {
      return PARSE_ERROR;
    }

    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i};
    node.isLeaf = true;

    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }
  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {

    if (literal.size() > sv.size()) {
      return PARSE_ERROR;
    }
    std::size_t i = 0;
    for (; i < literal.size(); i++) {
      if ((isCaseSensitive() ? sv[i] : tolower(sv[i])) != literal[i]) {
        return PARSE_ERROR;
      }
    }
    return i;
  }

  /// Create an insensitive Literal
  /// @return the insensitive Literal
  constexpr auto i() const noexcept { return Literal<toLower(), false>{}; }

private:
  static constexpr auto toLower() {
    decltype(literal) newLiteral;
    std::ranges::transform(literal, newLiteral.begin(),
                           [](char c) { return tolower(c); });
    return newLiteral;
  }

  static constexpr bool isCaseSensitive() {
    if (!case_sensitive) {
      return std::ranges::none_of(literal, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
      });
    }
    return case_sensitive;
  }
};
template <char_array_builder builder> consteval auto operator""_kw() {
  static_assert(!builder.value.empty(), "A keyword cannot be empty.");
  return Literal<builder.value>{};
}

template <typename... Elements>
  requires(isGrammarElement<std::decay_t<Elements>> && ...)
struct Group : GrammarElement {
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");
  constexpr ~Group() override = default;
  std::tuple<Elements...> elements;

  constexpr explicit Group(std::tuple<Elements...> elems)
      : elements{std::move(elems)} {}

  template <typename T>
  constexpr bool parse_rule_element(const T &element, std::string_view sv,
                                    CstNode &parent, Context &c,
                                    std::size_t size, std::size_t &i) const {

    auto len = element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
    if (fail(len)) {
      parent.content.resize(size);
      i = len;
      return false;
    }
    i += len;
    return true;
  }

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    std::size_t i = 0;

    std::apply(
        [&](const auto &...element) {
          auto size = parent.content.size();
          (parse_rule_element(element, sv, parent, c, size, i) && ...);
        },
        elements);

    return i;
  }

  template <typename T>
  constexpr bool parse_terminal_element(const T &element, std::string_view sv,
                                        std::size_t &i) const noexcept {

    auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
    if (fail(len)) {
      i = len;
      return false;
    }
    i += len;
    return true;
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t i = 0;

    std::apply(
        [&](const auto &...element) {
          (parse_terminal_element(element, sv, i) && ...);
        },
        elements);

    return i;
  }
};

template <typename... Lhs, typename... Rhs>
constexpr auto operator,(Group<Lhs...> lhs, Group<Rhs...> rhs) {
  return Group<std::decay_t<Lhs>..., std::decay_t<Rhs>...>{
      std::tuple_cat(lhs.elements, rhs.elements)};
}
template <typename... Lhs, typename Rhs>
constexpr auto operator,(Group<Lhs...> lhs, Rhs rhs) {
  return Group<std::decay_t<Lhs>..., std::decay_t<Rhs>>{
      std::tuple_cat(lhs.elements, std::make_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
constexpr auto operator,(Lhs lhs, Group<Rhs...> rhs) {
  return Group<std::decay_t<Lhs>, std::decay_t<Rhs>...>{
      std::tuple_cat(std::make_tuple(lhs), rhs.elements)};
}

template <typename Lhs, typename Rhs>
constexpr auto operator,(Lhs lhs, Rhs rhs) {
  return Group<std::decay_t<Lhs>, std::decay_t<Rhs>>{std::make_tuple(lhs, rhs)};
}

template <typename... Elements>
  requires(isGrammarElement<Elements> && ...)
struct UnorderedGroup : GrammarElement {
  static_assert(sizeof...(Elements) > 1,
                "An UnorderedGroup shall contains at least 2 elements.");
  constexpr ~UnorderedGroup() override = default;
  std::tuple<Elements...> elements;

  constexpr explicit UnorderedGroup(std::tuple<Elements...> elems)
      : elements{std::move(elems)} {}

  using ProcessedFlags = std::array<bool, sizeof...(Elements)>;

  template <typename T>
  static constexpr bool
  parse_rule_element(const T &element, std::string_view sv, CstNode &parent,
                     Context &c, std::size_t &i, ProcessedFlags &processed,
                     std::size_t index) noexcept {
    if (processed[index]) {
      return false;
    }

    if (auto len =
            element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
        success(len)) {
      i += len;
      processed[index] = true;
      return true;
    }
    return false;
  }

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    std::size_t i = 0;
    ProcessedFlags processed{};

    while (!std::ranges::all_of(processed, [](bool p) { return p; })) {
      bool anyProcessed = std::apply(
          [&](const auto &...element) {
            std::size_t index = 0;
            return (parse_rule_element(element, sv, parent, c, i, processed,
                                       index++) ||
                    ...);
          },
          elements);

      if (!anyProcessed) {
        break;
      }
    }
    return std::ranges::all_of(processed, [](bool p) { return p; })
               ? i
               : PARSE_ERROR;
  }

  template <typename T>
  static constexpr bool
  parse_terminal_element(const T &element, std::string_view sv, std::size_t &i,
                         ProcessedFlags &processed,
                         std::size_t index) noexcept {
    if (processed[index]) {
      return false;
    }

    if (auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
        success(len)) {
      i += len;
      processed[index] = true;
      return true;
    }
    return false;
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t i = 0;
    ProcessedFlags processed{};

    while (!std::ranges::all_of(processed, [](bool p) { return p; })) {
      bool anyProcessed = std::apply(
          [&](const auto &...element) {
            std::size_t index = 0;
            return (
                parse_terminal_element(element, sv, i, processed, index++) ||
                ...);
          },
          elements);

      if (!anyProcessed) {
        break;
      }
    }
    return std::ranges::all_of(processed, [](bool p) { return p; })
               ? i
               : PARSE_ERROR;
  }
};

template <typename... Lhs, typename... Rhs>
constexpr auto operator&(UnorderedGroup<Lhs...> lhs,
                         UnorderedGroup<Rhs...> rhs) {
  return UnorderedGroup<std::decay_t<Lhs>..., std::decay_t<Rhs>...>{
      std::tuple_cat(lhs.elements, rhs.elements)};
}
template <typename... Lhs, typename Rhs>
constexpr auto operator&(UnorderedGroup<Lhs...> lhs, Rhs rhs) {
  return UnorderedGroup<std::decay_t<Lhs>..., std::decay_t<Rhs>>{
      std::tuple_cat(lhs.elements, std::make_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
constexpr auto operator&(Lhs lhs, UnorderedGroup<Rhs...> rhs) {
  return UnorderedGroup<std::decay_t<Lhs>, std::decay_t<Rhs>...>{
      std::tuple_cat(std::make_tuple(lhs), rhs.elements)};
}
template <typename Lhs, typename Rhs>
constexpr auto operator&(Lhs lhs, Rhs rhs) {
  return UnorderedGroup<std::decay_t<Lhs>, std::decay_t<Rhs>>{
      std::make_tuple(lhs, rhs)};
}

template <typename... Elements>
  requires(isGrammarElement<Elements> && ...)
struct OrderedChoice : GrammarElement {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  constexpr ~OrderedChoice() override = default;
  std::tuple<Elements...> elements;

  constexpr explicit OrderedChoice(std::tuple<Elements...> elems)
      : elements{std::move(elems)} {}

  template <typename T>
  static constexpr bool
  parse_rule_element(const T &element, std::string_view sv, CstNode &parent,
                     Context &c, std::size_t size, std::size_t &i) noexcept {
    i = element.parse_rule(sv, parent, c);
    if (success(i)) {
      return true;
    }
    parent.content.resize(size);
    return false;
  }

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    std::size_t i = PARSE_ERROR;
    std::apply(
        [&](const auto &...element) {
          auto size = parent.content.size();
          (parse_rule_element(element, sv, parent, c, size, i) || ...);
        },
        elements);

    return i;
  }
  template <typename T>
  static constexpr bool parse_terminal_element(const T &element,
                                               std::string_view sv,
                                               std::size_t &i) noexcept {
    i = element.parse_terminal(sv);
    return success(i);
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t i = PARSE_ERROR;

    std::apply(
        [&](const auto &...element) {
          (parse_terminal_element(element, sv, i) || ...);
        },
        elements);
    return i;
  }
};
template <typename... Lhs, typename... Rhs>
constexpr auto operator|(OrderedChoice<Lhs...> lhs, OrderedChoice<Rhs...> rhs) {
  return OrderedChoice<std::decay_t<Lhs>..., std::decay_t<Rhs>...>{
      std::tuple_cat(lhs.elements, rhs.elements)};
}
template <typename... Lhs, typename Rhs>
  requires isGrammarElement<Rhs>
constexpr auto operator|(OrderedChoice<Lhs...> lhs, Rhs rhs) {
  return OrderedChoice<std::decay_t<Lhs>..., std::decay_t<Rhs>>{
      std::tuple_cat(lhs.elements, std::make_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
  requires isGrammarElement<Lhs>
constexpr auto operator|(Lhs lhs, OrderedChoice<Rhs...> rhs) {
  return OrderedChoice<std::decay_t<Lhs>, std::decay_t<Rhs>...>{
      std::tuple_cat(std::make_tuple(lhs), rhs.elements)};
}
template <typename Lhs, typename Rhs>
  requires isGrammarElement<Lhs> && isGrammarElement<Rhs>
constexpr auto operator|(Lhs lhs, Rhs rhs) {
  return OrderedChoice<std::decay_t<Lhs>, std::decay_t<Rhs>>{
      std::make_tuple(lhs, rhs)};
}

template <std::size_t min, std::size_t max, typename T>
  requires isGrammarElement<T>
struct Repetition : GrammarElement {
  constexpr ~Repetition() override = default;
  T element;
  constexpr explicit Repetition(T element) : element{element} {}

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    std::size_t count = 0;
    std::size_t i = 0;
    auto size = parent.content.size();
    while (count < min) {
      auto len = element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
      if (fail(len)) {
        parent.content.resize(size);
        return len;
      }
      i += len;
      count++;
    }
    while (count < max) {
      size = parent.content.size();
      auto len = element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
      if (fail(len)) {
        parent.content.resize(size);
        break;
      }
      i += len;
      count++;
    }
    return i;
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t count = 0;
    std::size_t i = 0;
    while (count < min) {
      auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
      if (fail(len)) {
        return len;
      }
      i += len;
      count++;
    }
    while (count < max) {
      auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
      if (fail(len)) {
        break;
      }
      i += len;
      count++;
    }
    return i;
  }
};

/// Create an option (zero or one)
/// @tparam ...Args
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename... Args>
  requires(isGrammarElement<Args> && ...)
constexpr auto opt(Args... args) {
  return Repetition<0, 1, std::decay_t<decltype((args, ...))>>{(args, ...)};
}

/// Create a repetition of zero or more elements
/// @tparam ...Args
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename... Args>
  requires(isGrammarElement<Args> && ...)
constexpr auto many(Args... args) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(),
                    std::decay_t<decltype((args, ...))>>{(args, ...)};
}

/// Create a repetition of one or more elements
/// @tparam ...Args
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename... Args>
  requires(isGrammarElement<Args> && ...)
constexpr auto at_least_one(Args... args) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(),
                    std::decay_t<decltype((args, ...))>>{(args, ...)};
}

/// Create a repetition of one or more elements with a separator
/// @tparam ...Args
/// @param sep the separator to be used between elements
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename Sep, typename... Args>
  requires isGrammarElement<Sep> && (isGrammarElement<Args> && ...)
constexpr auto at_least_one_sep(Sep sep, Args... args) {
  return (args, ...), many(sep, (args, ...));
}

/// Create a repetition of zero or more elements with a separator
/// @tparam ...Args
/// @param sep the separator to be used between elements
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename Sep, typename... Args>
  requires isGrammarElement<Sep> && (isGrammarElement<Args> && ...)
constexpr auto many_sep(Sep sep, Args... args) {
  return opt(at_least_one_sep(sep, args...));
}

/// Create a custom repetition with min and max.
/// @tparam ...Args
/// @param ...args a sequence of elements to be repeated
/// @param min the min number of occurence (inclusive)
/// @param max the maw number of occurence (inclusive)
/// @return The created Repetition
template <std::size_t min, std::size_t max, typename... Args>
  requires(isGrammarElement<Args> && ...)
constexpr auto rep(Args... args) {
  return Repetition<min, max, std::decay_t<decltype((args, ...))>>{(args, ...)};
}

/// Create a repetition of one or more elements
/// @tparam ...T
/// @param ...arg the element to be repeated
/// @return The created Repetition
template <typename T>
  requires isGrammarElement<T>
constexpr auto operator+(T arg) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(),
                    std::decay_t<T>>{arg};
}

/// Create a repetition of zero or more elements
/// @tparam ...T
/// @param ...arg the element to be repeated
/// @return The created Repetition
template <typename T>
  requires isGrammarElement<T>
constexpr auto operator*(T arg) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(),
                    std::decay_t<T>>{arg};
}

template <typename T>
  requires isGrammarElement<T>
struct AndPredicate : GrammarElement {
  constexpr ~AndPredicate() override = default;
  T element;
  explicit constexpr AndPredicate(T element) : element{element} {}

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &,
                                   Context &c) const noexcept override {
    CstNode node;
    return success(element.parse_rule(sv, node, c)) ? 0 : PARSE_ERROR;
  }
  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    return success(element.parse_terminal(sv)) ? 0 : PARSE_ERROR;
  }
};

template <typename T>
  requires isGrammarElement<T>
constexpr auto operator&(T element) {
  return AndPredicate<std::decay_t<T>>{element};
}

template <typename T>
  requires isGrammarElement<T>
struct NotPredicate : GrammarElement {
  constexpr ~NotPredicate() override = default;
  T element;
  explicit constexpr NotPredicate(T element) : element{element} {}
  constexpr std::size_t parse_rule(std::string_view sv, CstNode &,
                                   Context &c) const noexcept override {
    CstNode node;
    return success(element.parse_rule(sv, node, c)) ? PARSE_ERROR : 0;
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    return success(element.parse_terminal(sv)) ? PARSE_ERROR : 0;
  }
};

template <typename T>
  requires isGrammarElement<T>
constexpr auto operator!(T element) {
  return NotPredicate<std::decay_t<T>>{element};
}

struct AnyCharacter final : GrammarElement {
  constexpr ~AnyCharacter() override = default;

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    auto i = codepoint_length(sv);
    if (fail(i)) {
      return PARSE_ERROR;
    }
    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i};
    node.isLeaf = true;

    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }
  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    return codepoint_length(sv);
  }

private:
  static constexpr std::size_t codepoint_length(std::string_view sv) noexcept {
    if (!sv.empty()) {
      auto b = static_cast<std::byte>(sv.front());
      if ((b & std::byte{0x80}) == std::byte{0}) {
        return 1;
      }
      if ((b & std::byte{0xE0}) == std::byte{0xC0} && sv.size() >= 2) {
        return 2;
      }
      if ((b & std::byte{0xF0}) == std::byte{0xE0} && sv.size() >= 3) {
        return 3;
      }
      if ((b & std::byte{0xF8}) == std::byte{0xF0} && sv.size() >= 4) {
        return 4;
      }
    }
    return PARSE_ERROR;
  }
};

struct RuleCall final : GrammarElement {

  // here the rule parameter is a reference on the shared_ptr instead of on the
  // rule because the rule may not be allocated/initialized yet
  explicit RuleCall(const std::shared_ptr<Rule> &rule) : _rule(rule) {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const noexcept override {
    assert(_rule && "Call of an undefined rule.");
    return _rule->parse_rule(sv, parent, c);
  }

  std::size_t parse_terminal(std::string_view sv) const noexcept override {
    assert(_rule && "Call of an undefined rule.");
    return _rule->parse_terminal(sv);
  }

private:
  const std::shared_ptr<Rule> &_rule;
};

template <auto feature, typename Element>
struct Assignment final : GrammarElement {
  Element element;
  constexpr explicit Assignment(Element element) : element{element} {}
  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const noexcept override {
    CstNode node;
    auto i = element.parse_rule(sv, node, c);
    if (success(i)) {
      node.text = {sv.data(), i};
      node.grammarSource = this;
      parent.content.emplace_back(std::move(node));
    }
    return i;
  }
  constexpr std::size_t
  parse_terminal(std::string_view) const noexcept override {
    assert(false && "An Assignment cannot be in a terminal.");
    return PARSE_ERROR;
  }
};

/// Assign an element to a member of the current object
/// @tparam ...Args
/// @tparam e the member pointer
/// @param ...args the list of grammar elements
/// @return
template <auto e, typename... Args>
  requires(std::derived_from<Args, GrammarElement> && ...)
static constexpr auto assign(Args... args) {
  return Assignment<e, std::decay_t<decltype((args, ...))>>((args, ...));
}

/// Append an element to a member of the current object
/// @tparam ...Args
/// @tparam e the member pointer
/// @param ...args the list of grammar elements
/// @return
template <auto e, typename... Args>
  requires(std::derived_from<Args, GrammarElement> && ...)
static constexpr auto append(Args... args) {
  return Assignment<e, std::decay_t<decltype((args, ...))>>((args, ...));
}

/// any character equivalent to regex `.`
static constexpr AnyCharacter dot{};
/// The end of file token
static constexpr auto eof = !dot;
/// The end of line token
static constexpr auto eol = "\r\n"_kw | "\n"_kw | "\r"_kw;
/// a space character equivalent to regex `\s`
static constexpr auto s = " \t\r\n\f\v"_cr;
/// a non space character equivalent to regex `\S`
static constexpr auto S = !s;
/// a word character equivalent to regex `\w`
static constexpr auto w = "a-zA-Z0-9_"_cr;
/// a non word character equivalent to regex `\W`
static constexpr auto W = !w;
/// a digit character equivalent to regex `\d`
static constexpr auto d = "0-9"_cr;
/// a non-digit character equivalent to regex `\D`
static constexpr auto D = !d;

/// An until operation that starts from element `from` and ends to element
/// `to`. e.g `"#*"_kw >> "*#"_kw` to match a multiline comment
/// @param from the starting element
/// @param to the ending element
/// @return the until element
template <typename T, typename U>
  requires isGrammarElement<T> && isGrammarElement<U>
constexpr auto operator>>(T from, U to) {
  return (from, *(!to, dot), to);
}
} // namespace pegium::grammar