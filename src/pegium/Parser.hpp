

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
  /// any character equivalent to regex `.`
  inline std::shared_ptr<AnyCharacter> any() const {
    return std::make_shared<AnyCharacter>();
  }

  /// a space character equivalent to regex `\s`
  inline std::shared_ptr<CharacterClass> s() const {
    return std::make_shared<CharacterClass>(" \t\r\n\f\v", false, false);
  }
  /// a non space character equivalent to regex `\S`
  inline std::shared_ptr<CharacterClass> S() const {
    return std::make_shared<CharacterClass>(" \t\r\n\f\v", true, false);
  }

  /// a word character equivalent to regex `\w`
  inline std::shared_ptr<CharacterClass> w() const {
    return std::make_shared<CharacterClass>("a-zA-Z0-9_", false, false);
  }
  /// a non word character equivalent to regex `\W`
  inline std::shared_ptr<CharacterClass> W() const {
    return std::make_shared<CharacterClass>("a-zA-Z0-9_", true, false);
  }
  /// a digit character equivalent to regex `\d`
  inline std::shared_ptr<CharacterClass> d() const {
    return std::make_shared<CharacterClass>("0-9", false, false);
  }
  /// a non-digit character equivalent to regex `\D`
  inline std::shared_ptr<CharacterClass> D() const {
    return std::make_shared<CharacterClass>("0-9", true, false);
  }

  inline std::shared_ptr<Character> chr(char dt) const {
    return std::make_shared<Character>(dt);
  }

  inline std::shared_ptr<CharacterClass>
  cls(std::string_view s, bool negated = false, bool ignoreCase = false) const {
    return std::make_shared<CharacterClass>(s, negated, ignoreCase);
  }

  /// Create a repetion of zero or more elements
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  inline std::shared_ptr<Many> many(Args... args) const {
    return std::make_shared<Many>((std::move(args), ...));
  }
  /// Create a repetion of zero or more elements with a separator
  /// @tparam ...Args
  /// @param sep the separator to be used between elements
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  inline std::shared_ptr<Optional> many_sep(std::shared_ptr<GrammarElement> sep,
                                            Args... args) const {
    return std::make_shared<Optional>(at_least_one_sep(sep, args...));
  }

  /// Create a repetion of one or more elements
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  inline std::shared_ptr<AtLeastOne> at_least_one(Args &&...args) const {
    return std::make_shared<AtLeastOne>((std::forward<Args>(args), ...));
  }

  /// Create a repetion of one or more elements with a separator
  /// @tparam ...Args
  /// @param sep the separator to be used between elements
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  inline std::shared_ptr<Group>
  at_least_one_sep(std::shared_ptr<GrammarElement> sep, Args... args) const {
    return ((args, ...), std::make_shared<Many>((sep, (args, ...))));
  }
  /// Create an option (zero or one)
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @return The created Repetition
  template <typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  inline std::shared_ptr<Optional> opt(Args &&...args) const {
    return std::make_shared<Optional>((std::forward<Args>(args), ...));
  }
  /// Create a custom repetion with min and max.
  /// @tparam ...Args
  /// @param ...args a sequence of elements to be repeated
  /// @param min the min number of occurence (inclusive)
  /// @param max the maw number of occurence (inclusive)
  /// @return The created Repetition
  template <typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  inline std::shared_ptr<Repetition> rep(size_t min, size_t max,
                                         Args &&...args) const {
    return std::make_shared<Repetition>((std::forward<Args>(args), ...), min,
                                        max);
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
  std::shared_ptr<Action> action(R C::*member) const {

    return std::make_shared<Action>([member](std::any &value) {
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
  template <typename C, typename R>
  std::shared_ptr<Action> action(R C::*member) const {
    return std::make_shared<Action>([member](std::any &value) {
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
  template <typename T> std::shared_ptr<Action> action() const {
    return std::make_shared<Action>([](std::any &value) {
      // create a new object of type T and assign it to value
      value = std::static_pointer_cast<AstNode>(std::make_shared<T>());
    });
  }

  template <typename T = std::string, typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  void rule(std::string name, Args &&...args) {
    _rules[name] = std::make_shared<ParserRule>(
        name, [this] { return this->createContext(); },
        (std::forward<Args>(args), ...), make_converter<T>(),
        Rule::DataTypeOf<T>());
  }

  template <typename T = std::string, typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  void terminal(std::string name, Args &&...args) {
    _rules[name] = std::make_shared<TerminalRule>(
        name, [this] { return this->createContext(); },
        (std::forward<Args>(args), ...), TerminalRule::Kind::Normal,
        make_converter<T>(), Rule::DataTypeOf<T>());
  }

  template <typename T = std::string, typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  void hidden_terminal(std::string name, Args &&...args) {
    _rules[name] = std::make_shared<TerminalRule>(
        name, [this] { return this->createContext(); },
        (std::forward<Args>(args), ...), TerminalRule::Kind::Hidden,
        make_converter<T>(), Rule::DataTypeOf<T>());
  }

  template <typename T = std::string, typename... Args>
    requires(std::convertible_to<Args, std::shared_ptr<GrammarElement>> && ...)
  void ignored_terminal(std::string name, Args &&...args) {
    _rules[name] = std::make_shared<TerminalRule>(
        name, [this] { return this->createContext(); },
        (std::forward<Args>(args), ...), TerminalRule::Kind::Ignored,
        make_converter<T>(), Rule::DataTypeOf<T>());
  }

  std::shared_ptr<GrammarElement> call(const std::string &name) {
    return std::make_shared<RuleCall>(_rules[name]);
  }

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
} // namespace pegium