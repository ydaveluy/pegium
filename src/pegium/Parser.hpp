

#pragma once
#include <map>
#include <pegium/IParser.hpp>
#include <pegium/grammar.hpp>
#include <pegium/syntax-tree.hpp>
#include <string>
#include <string_view>

namespace pegium {

template <typename T>
concept ValidRuleType = std::is_same_v<T, bool> ||        // bool
                        std::is_same_v<T, char> ||        // char
                        std::is_same_v<T, int8_t> ||      // int8
                        std::is_same_v<T, int16_t> ||     // int16
                        std::is_same_v<T, int32_t> ||     // int32
                        std::is_same_v<T, int64_t> ||     // int64
                        std::is_same_v<T, uint8_t> ||     // uint8
                        std::is_same_v<T, uint16_t> ||    // uint16
                        std::is_same_v<T, uint32_t> ||    // uint32
                        std::is_same_v<T, uint64_t> ||    // uint64
                        std::is_same_v<T, float> ||       // float
                        std::is_same_v<T, double> ||      // double
                        std::is_enum_v<T> ||              //  enum
                        std::is_same_v<T, std::string> || // string
                        std::is_base_of_v<AstNode, T>;    // AstNode

class Parser : public IParser {
public:
  ParseResult parse(const std::string &input) const override;
  ParseResult parse(const std::string &name, std::string_view text) const;
  ~Parser() noexcept override = default;

protected:
  /// The end of file token
  static NotPredicate eof() { return !AnyCharacter{}; }

  /// The end of line token
  static PrioritizedChoice eol() { return "\r\n"_kw | '\n'_kw | '\r'_kw; }

  /// any character equivalent to regex `.`
  static inline AnyCharacter any() { return {}; }

  /// a space character equivalent to regex `\s`
  static inline CharacterClass s() {
    return CharacterClass(" \t\r\n\f\v", false, false);
  }
  /// a non space character equivalent to regex `\S`
  static inline CharacterClass S() {
    return CharacterClass(" \t\r\n\f\v", true, false);
  }

  /// a word character equivalent to regex `\w`
  static inline CharacterClass w() {
    return CharacterClass("a-zA-Z0-9_", false, false);
  }
  /// a non word character equivalent to regex `\W`
  static inline CharacterClass W() {
    return CharacterClass("a-zA-Z0-9_", true, false);
  }
  /// a digit character equivalent to regex `\d`
  static inline CharacterClass d() {
    return CharacterClass("0-9", false, false);
  }
  /// a non-digit character equivalent to regex `\D`
  static inline CharacterClass D() {
    return CharacterClass("0-9", true, false);
  }

  static inline Character chr(char dt) { return Character(dt); }

  static inline CharacterClass cls(std::string_view s, bool negated = false,
                                   bool ignoreCase = false) {
    return CharacterClass(s, negated, ignoreCase);
  }

  /// Create a repetition of zero or more elements
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  static inline Many many(Args &&...args) {
    return Many((std::forward<Args>(args), ...));
  }
  /// Create a repetition of zero or more elements with a separator
  /// @tparam ...Args
  /// @param sep the separator to be used between elements
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename Sep, typename... Args>
    requires IsGrammarElement<Sep> && (IsGrammarElement<Args> && ...)
  static inline Optional many_sep(Sep &&sep, Args &&...args) {
    return Optional(
        at_least_one_sep(std::forward<Sep>(sep), std::forward<Args>(args)...));
  }

  /// Create a repetition of one or more elements
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  static inline AtLeastOne at_least_one(Args &&...args) {
    return AtLeastOne((std::forward<Args>(args), ...));
  }

  /// Create a repetition of one or more elements with a separator
  /// @tparam ...Args
  /// @param sep the separator to be used between elements
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition

  template <typename Sep, typename... Args>
    requires IsGrammarElement<Sep> && (IsGrammarElement<Args> && ...)
  static inline Group at_least_one_sep(Sep &&sep, Args... args) {
    return Group((args, ...), Many{(std::forward<Sep>(sep), (args, ...))});
  }
  /// Create an option (zero or one)
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  static inline Optional opt(Args &&...args) {
    return Optional((std::forward<Args>(args), ...));
  }
  /// Create a custom repetition with min and max.
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @param min the min number of occurence (inclusive)
  /// @param max the maw number of occurence (inclusive)
  /// @return The created Repetition
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  static inline Repetition rep(size_t min, size_t max, Args &&...args) {
    return Repetition((std::forward<Args>(args), ...), min, max);
  }

  /// An action that create a new object of type T and assign its member
  /// with the current value.
  /// ```
  /// action<AstDerived>(&AstBase::member)
  /// ```
  /// @tparam T the object type
  /// @tparam C the member class type
  /// @tparam R the member type
  /// @param member the object member
  /// @return the created Action.
  template <typename T, typename C, typename R>
    requires std::derived_from<T, C>
  static Action action(R C::*member) {

    return Action([member](std::any &value) {
      // create a new object of type C
      auto result = std::make_shared<T>();
      // assign the current value to the new object member
      Feature{member}.assign(result, value);
      // re-assign the value to the new object
      value = std::static_pointer_cast<AstNode>(result);
    });
  }

  /// An action that create a new object of type C and assign its member
  /// with the current value.
  /// ```
  /// action(&AstBase::member)
  /// ```
  /// @tparam C the object type
  /// @tparam R the member type
  /// @param member the object member
  /// @return the created Action.
  template <typename C, typename R> static Action action(R C::*member) {
    return Action([member](std::any &value) {
      // create a new object of type C
      auto result = std::make_shared<C>();
      // assign the current value to the new object member
      Feature{member}.assign(result, value);
      // re-assign the value to the new object
      value = std::static_pointer_cast<AstNode>(result);
    });
  }

  /// An action that create an object of type T
  /// ```
  /// action<AstBase>()
  /// ```
  /// @tparam T the object type
  /// @return the created Action.
  template <typename T> static Action action() {
    return Action([](std::any &value) {
      // create a new object of type T and assign it to value
      value = std::static_pointer_cast<AstNode>(std::make_shared<T>());
    });
  }

  template <typename T> // TODO add requires T DataType or AstNode
    requires std::derived_from<T, AstNode>
  ParserRule &rule(std::string name) {
    auto rule = std::make_shared<ParserRule>(
        name, [this] { return this->createContext(); }, make_converter<T>());

    _rules[name] = rule;
    return *rule.get();
  }

  template <typename T = std::string>
    requires is_data_type_v<T>
  DataTypeRule &rule(std::string name) {
    auto rule = std::make_shared<DataTypeRule>(
        name, [this] { return this->createContext(); }, make_converter<T>(),
        data_type_of<T>());

    _rules[name] = rule;
    return *rule.get();
  }

  template <typename T = std::string> TerminalRule &terminal(std::string name) {

    auto rule = std::make_shared<TerminalRule>(
        name, [this] { return this->createContext(); }, make_converter<T>(),
        data_type_of<T>());

    _rules[name] = rule;
    return *rule.get();
  }

  /// Call an other rule
  /// @param name the rule name
  /// @return the Rule call action
  RuleCall call(const std::string &name) { return RuleCall(_rules[name]); }

private:
  Context createContext() const;

  template <typename T>
  std::function<bool(std::any &, CstNode &)> make_converter() const {
    return [](std::any &value, CstNode &node) {
      if constexpr (std::is_base_of_v<AstNode, T>) {
        value = std::static_pointer_cast<AstNode>(std::make_shared<T>());
        return true;
      } else {
        std::string result;
        for (const auto &n : node)
          if (n.isLeaf && !n.hidden)
            result += n.text;

        value = result;
        return false;
      }
    };
  }
  std::map<std::string, std::shared_ptr<Rule>, std::less<>> _rules;
};

/// An until operation that starts from element `from` and ends to element
/// `to`. e.g `"/*"_kw >> "*/"_kw` to match a multiline comment
/// @param from the starting element
/// @param to the ending element
/// @return the until element
template <typename T, typename U>
  requires IsGrammarElement<T> && IsGrammarElement<U>
Group operator>>(T &&from, U to) {
  return Group(std::forward<T>(from), Many{(!to, AnyCharacter{})}, to);
}

/// Create a repetition of one or more elements
/// @tparam ...Args
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename Args>
  requires(IsGrammarElement<Args>)
inline AtLeastOne operator+(Args &&args) {
  return AtLeastOne(std::forward<Args>(args));
}

/// Create a repetition of zero or more elements
/// @tparam ...Args
/// @param ...args a sequence of elements to be repeated
/// @return The created Repetition
template <typename Args>
  requires IsGrammarElement<Args>
inline Many operator*(Args &&args) {
  return Many(std::forward<Args>(args));
}

namespace regex {
struct Element : public AstNode {};
struct Many : public Element {
  attribute<Element> element;
};
struct Optional : public Element {
  attribute<Element> element;
};
struct AtLeastOne : public Element {
  attribute<Element> element;
};
struct Repetition : public Element {
  attribute<Element> element;
  attribute<int64_t> min = 0;
  attribute<int64_t> max = std::numeric_limits<int64_t>::max();
};

struct CharacterClass : public Element {
  attribute<string> value;
};
struct StringLiteral : public Element {
  attribute<string> value;
};

class RegexParser : public Parser {
  RegexParser() {
    rule<Many>("Many")(call("Element"), '*'_kw);
    rule<Optional>("Optional")(call("Element"), '?'_kw);
    rule<AtLeastOne>("AtLeastOne")(call("Element"), '+'_kw);

    rule<Repetition>("Repetition")(call("Element"), '{'_kw,
                                   &Repetition::min += +d(), ','_kw,
                                   &Repetition::max += + +d(), '}'_kw);

    rule<CharacterClass>(
        "CharacterClass")(&CharacterClass::value +=
                          ('['_kw, many(R"(\\)"_kw | R"(\])"_kw | !']'_kw),
                           ']'_kw));

    rule<StringLiteral>(
        "StringLiteral")(&StringLiteral::value +=
                         ('['_kw, many(R"(\\)"_kw | R"(\])"_kw | !']'_kw),
                          ']'_kw));
  }
};

// static constexpr RegexParser p;
} // namespace regex
// Opérateur de string literal ""_reg
consteval auto operator""_reg(const char *str, std::size_t len) {

  std::string_view input{str, len};
}

} // namespace pegium