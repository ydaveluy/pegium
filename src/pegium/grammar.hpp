#pragma once

#include <cctype>
#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <pegium/syntax-tree.hpp>
#include <pegium/IParser.hpp>

namespace pegium {

class Context;
class Group;
class PrioritizedChoice;
class Repetition;
class AndPredicate;
class NotPredicate;
class Keyword;
class ParserRule;
class RuleCall;
class AnyCharacter;
class TerminalRule;
class Character;
class CharacterClass;
class Assignment;
class Action;

class GrammarElement {
public:
  struct Visitor {
    virtual ~Visitor() noexcept = default;
    virtual void visit(const Group &) {}
    virtual void visit(const PrioritizedChoice &) {}
    virtual void visit(const Repetition &) {}
    virtual void visit(const AndPredicate &) {}
    virtual void visit(const NotPredicate &) {}
    virtual void visit(const Keyword &) {}
    virtual void visit(const ParserRule &) {}
    virtual void visit(const RuleCall &) {}
    virtual void visit(const AnyCharacter &) {}
    virtual void visit(const TerminalRule &) {}
    virtual void visit(const Character &) {}
    virtual void visit(const CharacterClass &) {}
    virtual void visit(const Assignment &) {}
    virtual void visit(const Action &) {}
  };
  struct Result {
    std::size_t len;
    std::shared_ptr<CstNode> cstNode;
  };

  virtual ~GrammarElement() = default;

  virtual void accept(Visitor &v) const = 0;

  // parse the input text from a rule: hidden/ignored token between elements are
  // skipped
  virtual std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                 Context &c) const = 0;

  // parse the input text from a terminal: no hidden/ignored token between
  // elements
  virtual std::size_t parse_terminal(std::string_view sv) const = 0;

  // parse the input text for hidden rules: ignored token are ignored, hidden
  // token are converted to hidden CstNode
  virtual std::size_t parse_hidden(std::string_view sv,
                                   CstNode &parent) const = 0;
};

/// A no operation
class NoOp : public GrammarElement {
public:
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  void accept(Visitor &v) const override;
};

class Context {
public:
  explicit Context(
      std::shared_ptr<GrammarElement> hidden = std::make_shared<NoOp>());

  std::size_t skipHiddenNodes(std::string_view sv, CstNode &node) const;

private:
  std::shared_ptr<GrammarElement> hidden;
};
using ContextProvider = std::function<Context()>;

struct Feature {

  template <typename R, typename C> static constexpr auto equalOperator() {
    return [](const Feature &lhs, const Feature &rhs) {
      return rhs._feature.type() == typeid(R C::*) &&
             std::any_cast<R C::*>(lhs._feature) ==
                 std::any_cast<R C::*>(rhs._feature);
    };
  }
  template <typename R, typename C> static constexpr auto assignOperator() {
    if constexpr (std::is_same_v<R, std::shared_ptr<AstNode>> ||
                  std::is_same_v<R, std::vector<std::shared_ptr<AstNode>>>) {
      return [](AstNode *object, const std::any &fea, const std::any &value) {
        auto *obj = dynamic_cast<C *>(object);
        assert(obj);
        auto &member = obj->*std::any_cast<R C::*>(fea);

        if constexpr (std::is_same_v<R, std::shared_ptr<AstNode>>) {
          member = std::dynamic_pointer_cast<R::element_type>(
              std::any_cast<std::shared_ptr<AstNode>>(value));
        } else {
          member.emplace_back(
              std::dynamic_pointer_cast<R::value_type::element_type>(
                  std::any_cast<std::shared_ptr<AstNode>>(value)));
        }
      };
    } else if constexpr (std::is_same_v<R, std::vector<AstNode>>) {
      return [](AstNode *object, const std::any &fea, const std::any &value) {
        auto *obj = dynamic_cast<C *>(object);
        assert(obj);
        auto &member = obj->*std::any_cast<R C::*>(fea);
        member.emplace_back(std::any_cast<AstNode>(value));
      };
    } else {
      return [](AstNode *object, const std::any &fea, const std::any &value) {
        auto *obj = dynamic_cast<C *>(object);
        assert(obj);
        auto &member = obj->*std::any_cast<R C::*>(fea);
        member = std::any_cast<R>(value);
      };
    }
  }

  template <typename R, typename C, typename Assign>
  Feature(R C::*feature, Assign &&assign)
      : _feature{feature}, _equal{equalOperator<std::shared_ptr<R>, C>()},
        _assign{std::forward<Assign>(assign)} {}

  template <typename R, typename C>
    requires std::derived_from<C, AstNode> // TODO && R is "simple type"
  explicit Feature(R C::*feature)
      : Feature{feature, [](AstNode *object, const std::any &fea,
                            const std::any &value) {
                  auto *obj = dynamic_cast<C *>(object);
                  assert(obj);
                  auto &member = obj->*std::any_cast<R C::*>(fea);
                  member = std::any_cast<R>(value);
                }} {}

  template <typename R, typename C>
    requires std::derived_from<C, AstNode> // TODO && R is "simple type"
  explicit Feature(std::vector<R> C::*feature)
      : Feature{feature, [](AstNode *object, const std::any &fea,
                            const std::any &value) {
                  auto *obj = dynamic_cast<C *>(object);
                  assert(obj);
                  auto &member = obj->*std::any_cast<R C::*>(fea);
                  member.emplace_back(std::any_cast<R>(value));
                }} {}

  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && std::derived_from<R, AstNode>
  explicit Feature(std::shared_ptr<R> C::*feature)
      : Feature{feature, [](AstNode *object, const std::any &fea,
                            const std::any &value) {
                  auto *obj = dynamic_cast<C *>(object);
                  assert(obj);
                  auto &member =
                      obj->*std::any_cast<std::shared_ptr<R> C::*>(fea);
                  member = std::dynamic_pointer_cast<R>(
                      std::any_cast<std::shared_ptr<AstNode>>(value));
                }} {}
  template <typename R, typename C>
    requires std::derived_from<C, AstNode> && std::derived_from<R, AstNode>
  explicit Feature(std::vector<std::shared_ptr<R>> C::*feature)
      : Feature{feature, [](AstNode *object, const std::any &fea,
                            const std::any &value) {
                  auto *obj = dynamic_cast<C *>(object);
                  assert(obj);
                  auto &member =
                      obj->*std::any_cast<std::vector<std::shared_ptr<R>> C::*>(
                                fea);
                  member.emplace_back(std::dynamic_pointer_cast<R>(
                      std::any_cast<std::shared_ptr<AstNode>>(value)));
                }} {}

  bool operator==(const Feature &rhs) const noexcept;
  void assign(const std::any &object, const std::any &value) const;
  void assign(AstNode *object, const std::any &value) const;

  std::shared_ptr<Assignment> operator=(std::shared_ptr<GrammarElement> elem);

private:
  std::any _feature;
  std::function<bool(const Feature &lhs, const Feature &rhs)> _equal;

  std::function<void(AstNode *object, const std::any &feature,
                     const std::any &value)>
      _assign;
};

class Assignment : public GrammarElement {
public:
  Assignment(Feature feature, std::shared_ptr<GrammarElement> elem);

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  void accept(Visitor &v) const override;
  const Feature &getFeature() const noexcept { return feature; }

private:
  Feature feature;
  std::shared_ptr<GrammarElement> elem;
};

template <typename R, typename C>
std::shared_ptr<Assignment> operator+=(R C::*member,
                                       std::shared_ptr<GrammarElement> elem) {
  return std::make_shared<Assignment>(Feature{member}, std::move(elem));
}

class Action : public GrammarElement {
public:
  template <typename Func>
    requires std::invocable<Func, std::any &>
  explicit Action(Func &&func) : _action{std::forward<Func>(func)} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  void accept(Visitor &v) const override;

  void execute(std::any &data) const { _action(data); }

private:
  std::function<void(std::any &)> _action;
};

class Character : public GrammarElement {
public:
  explicit Character(char c);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  char _char;
};

class Keyword : public GrammarElement {
public:
  explicit Keyword(std::string s, bool ignore_case = false);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  std::string _kw;
  bool _ignore_case;
};

class Group : public GrammarElement {
public:
  template <typename... Args>
    requires((std::same_as<std::shared_ptr<GrammarElement>, Args> ||
              std::convertible_to<Args, std::shared_ptr<GrammarElement>>) &&
             ...)
  explicit Group(Args... args) : _elements{args...} {}
  explicit Group(std::shared_ptr<Group> seq) noexcept;
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  std::vector<std::shared_ptr<GrammarElement>> _elements;
  friend std::shared_ptr<Group> &operator,(std::shared_ptr<Group> &lhs,
                                           std::shared_ptr<GrammarElement> rhs);
};

std::shared_ptr<Group> operator,(std::shared_ptr<GrammarElement> lhs,
                                 std::shared_ptr<GrammarElement> rhs);

class Optional : public GrammarElement {
public:
  explicit Optional(std::shared_ptr<GrammarElement> element);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override {}

private:
  std::shared_ptr<GrammarElement> _element;
};
class Many : public GrammarElement {
public:
  explicit Many(std::shared_ptr<GrammarElement> element);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override {}

private:
  std::shared_ptr<GrammarElement> _element;
};
class AtLeastOne : public GrammarElement {
public:
  explicit AtLeastOne(std::shared_ptr<GrammarElement> element);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override {}

private:
  std::shared_ptr<GrammarElement> _element;
};

class Repetition : public GrammarElement {
public:
  Repetition(std::shared_ptr<GrammarElement> element, size_t min, size_t max);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

  bool is_many() const;

private:
  std::shared_ptr<GrammarElement> _element;
  size_t _min;
  size_t _max;
};

class PrioritizedChoice : public GrammarElement {
public:
  explicit PrioritizedChoice(
      std::vector<std::shared_ptr<GrammarElement>> &&elements);

  template <typename... Args>
    requires((std::same_as<std::shared_ptr<GrammarElement>, Args> ||
              std::convertible_to<Args, std::shared_ptr<GrammarElement>>) &&
             ...)
  explicit PrioritizedChoice(Args... args) : _elements{args...} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;

  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::vector<std::shared_ptr<GrammarElement>> _elements;
  friend std::shared_ptr<PrioritizedChoice> &
  operator|(std::shared_ptr<PrioritizedChoice> &lhs,
            std::shared_ptr<GrammarElement> rhs);
};

std::shared_ptr<PrioritizedChoice>
operator|(std::shared_ptr<GrammarElement> lhs,
          std::shared_ptr<GrammarElement> rhs);
class AndPredicate : public GrammarElement {
public:
  explicit AndPredicate(std::shared_ptr<GrammarElement> element);

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::shared_ptr<GrammarElement> _element;
};

class NotPredicate : public GrammarElement {
public:
  explicit NotPredicate(std::shared_ptr<GrammarElement> element);

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::shared_ptr<GrammarElement> _element;
};

class AnyCharacter : public GrammarElement {
public:
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;
};

class CharacterClass : public GrammarElement {
public:
  CharacterClass(std::string_view s, bool negated, bool ignore_case);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;

  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;

  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::array<bool, 256> lookup{};
  std::string _name;
};

class Grammar;



class Rule : public GrammarElement {
public:
  virtual ParseResult parse(std::string_view sv) const = 0;
  const std::string &name() const noexcept;

  enum class DataType {
    Bool,
    Char,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    Enum,
    String,
    AstNode
  };
  template <typename T> static constexpr DataType DataTypeOf() {
    if constexpr (std::is_same_v<T, bool>)
      return DataType::Bool;
    else if constexpr (std::is_same_v<T, char>)
      return DataType::Char;
    else if constexpr (std::is_same_v<T, int8_t>)
      return DataType::Int8;
    else if constexpr (std::is_same_v<T, int16_t>)
      return DataType::Int16;
    else if constexpr (std::is_same_v<T, int32_t>)
      return DataType::Int32;
    else if constexpr (std::is_same_v<T, int64_t>)
      return DataType::Int64;
    else if constexpr (std::is_same_v<T, uint8_t>)
      return DataType::UInt8;
    else if constexpr (std::is_same_v<T, uint16_t>)
      return DataType::UInt16;
    else if constexpr (std::is_same_v<T, uint32_t>)
      return DataType::UInt32;
    else if constexpr (std::is_same_v<T, uint64_t>)
      return DataType::UInt64;
    else if constexpr (std::is_same_v<T, float>)
      return DataType::Float;
    else if constexpr (std::is_same_v<T, double>)
      return DataType::Double;
    else if constexpr (std::is_enum_v<T>)
      return DataType::Enum;
    else if constexpr (std::is_same_v<T, std::string>)
      return DataType::String;
    else if constexpr (std::is_base_of_v<AstNode, T>)
      return DataType::AstNode;
    else
      static_assert("Unsupported type");
  }
  inline DataType getDataType() const noexcept { return _type; }

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  bool execute(std::any &data, CstNode &node) const {
    return _action(data, node);
  }

protected:
  explicit Rule(std::string_view name, ContextProvider provider,
                std::shared_ptr<GrammarElement> element,
                std::function<bool(std::any &, CstNode &)> action,
                DataType type);

private:
friend class ParserRule;
friend class TerminalRule;

  std::string _name;
  ContextProvider _context_provider;
  std::shared_ptr<GrammarElement> _element;
  /// the action to be called when invocating make_ast
  std::function<bool(std::any &, CstNode &)> _action;

  DataType _type;
};

class TerminalRule : public Rule {
public:

  ParseResult parse(std::string_view sv) const override;
  enum class Kind {
    // a terminal mapped to a normal CstNode (non-hidden)
    Normal,
    // a terminal mapped to an hidden CstNode
    Hidden,
    // a terminal not mapped to a CstNode
    Ignored
  };
  explicit TerminalRule(std::string_view name, ContextProvider provider,
                        std::shared_ptr<GrammarElement> element, Kind kind,
                        std::function<bool(std::any &, CstNode &)> action,
                        DataType type);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;

  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;

  void accept(Visitor &v) const override;
  Kind getKind() const;

private:
  Kind _kind;
};

class ParserRule : public Rule {
public:
  explicit ParserRule(std::string_view name, ContextProvider provider,
                      std::shared_ptr<GrammarElement> element,
                      std::function<bool(std::any &, CstNode &)> action,
                      DataType type);
  ParserRule(const ParserRule &) = delete;
  ParserRule(ParserRule &&) = delete;

  void accept(Visitor &v) const override;

  ParseResult parse(std::string_view sv) const override;
};

class RuleCall : public GrammarElement {
public:
  // here the rule parameter is a reference on the unique_ptr instead of on the
  // rule because the rule may not be allocated/initialized yet
  explicit RuleCall(const std::shared_ptr<Rule> &rule);

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;

  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  const std::shared_ptr<Rule> &_rule;
};

std::shared_ptr<GrammarElement> operator"" _kw(const char *str, std::size_t s);
std::shared_ptr<GrammarElement> operator"" _ikw(const char *str, std::size_t s);

std::shared_ptr<Character> operator"" _kw(char chr);

inline std::shared_ptr<AndPredicate>
operator&(std::shared_ptr<GrammarElement> ope) {
  return std::make_shared<AndPredicate>(std::move(ope));
}

inline std::shared_ptr<NotPredicate>
operator!(std::shared_ptr<GrammarElement> ope) {
  return std::make_shared<NotPredicate>(std::move(ope));
}

} // namespace pegium