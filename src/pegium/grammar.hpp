#pragma once

#include <cassert>
#include <cctype>
#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <pegium/IParser.hpp>
#include <pegium/syntax-tree.hpp>

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
class DataTypeRule;

class GrammarElement {
public:
  struct Visitor {
    virtual ~Visitor() noexcept = default;
    virtual void visit(const Group &) { /* ignore */ }
    virtual void visit(const PrioritizedChoice &) { /* ignore */ }
    virtual void visit(const Repetition &) { /* ignore */ }
    virtual void visit(const AndPredicate &) { /* ignore */ }
    virtual void visit(const NotPredicate &) { /* ignore */ }
    virtual void visit(const Keyword &) { /* ignore */ }
    virtual void visit(const RuleCall &) { /* ignore */ }
    virtual void visit(const AnyCharacter &) { /* ignore */ }
    virtual void visit(const Character &) { /* ignore */ }
    virtual void visit(const CharacterClass &) { /* ignore */ }
    virtual void visit(const Assignment &) { /* ignore */ }
    virtual void visit(const Action &) { /* ignore */ }

    virtual void visit(const ParserRule &) { /* ignore */ }
    virtual void visit(const DataTypeRule &) { /* ignore */ }
    virtual void visit(const TerminalRule &) { /* ignore */ }
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

protected:
  /// Utility to convert a GrammarElement to a shared_ptr
  /// @tparam T the element type
  /// @param param the grammar element
  /// @return a srared_ptr initialized with param
  template <typename T>
    requires std::derived_from<std::decay_t<T>, GrammarElement>
  static constexpr std::shared_ptr<std::decay_t<T>> make_shared(T &&param) {
    return std::make_shared<std::decay_t<T>>(std::forward<T>(param));
  }
};

/// Concept to check if a type is a GrammarElement
template <typename T>
concept IsGrammarElement = std::derived_from<std::decay_t<T>, GrammarElement>;

class Keyword final : public GrammarElement {
public:
  Keyword(Keyword &&) = default;
  Keyword(const Keyword &) = default;
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

class Character final : public GrammarElement {
public:
  Character(Character &&) = default;
  Character(const Character &) = default;
  explicit Character(char c);
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  char _char;
};

/// A no operation
class NoOp final : public GrammarElement {
public:
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  void accept(Visitor &v) const override;
};

class Context final {
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
                  auto &member = obj->*std::any_cast<std::vector<R> C::*>(fea);
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

  template <typename T>
    requires IsGrammarElement<T>
  Assignment operator=(T &&elem);

private:
  std::any _feature;
  std::function<bool(const Feature &lhs, const Feature &rhs)> _equal;

  std::function<void(AstNode *object, const std::any &feature,
                     const std::any &value)>
      _assign;
};

class Assignment final : public GrammarElement {
public:
  Assignment(Assignment &&) = default;
  Assignment(const Assignment &) = default;
  template <typename T>
    requires IsGrammarElement<T>
  Assignment(Feature feature, T &&elem)
      : feature{std::move(feature)}, elem{make_shared(std::forward<T>(elem))} {}

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

template <typename T>
  requires IsGrammarElement<T>
Assignment Feature::operator=(T &&elem) {
  return Assignment(*this, std::forward<T>(elem));
}

template <typename R, typename C, typename T>
  requires IsGrammarElement<T>
Assignment operator+=(R C::*member, T &&elem) {
  return Feature{member} = std::forward<T>(elem);
}

class Action final : public GrammarElement {
public:
  Action(Action &&) = default;
  Action(const Action &) = default;
  template <typename Func>
    requires std::invocable<Func, std::any &> &&
             (!std::same_as<std::decay_t<Func>, Action>)
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

class Group final : public GrammarElement {
public:
  Group(Group &&) = default;
  Group(const Group &) = default;
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...) &&
            (!std::same_as<std::decay_t<Args>, Group> && ...)
  explicit Group(Args &&...args)
      : _elements{make_shared(std::forward<Args>(args))...} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  std::vector<std::shared_ptr<GrammarElement>> _elements;

  // concat 2 Groups
  friend Group &&operator,(Group &&lhs, Group &&rhs) {
    for (auto &elem : rhs._elements)
      lhs._elements.emplace_back(std::move(elem));
    return std::move(lhs);
  }

  // append an element to an existing Group
  template <typename T>
    requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, Group>)
  friend Group &&operator,(Group &&lhs, T &&rhs) {
    lhs._elements.emplace_back(make_shared(std::forward<T>(rhs)));
    return std::move(lhs);
  }

  // prepend an element to an existing Group
  template <typename T>
    requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, Group>)
  friend Group operator,(T &&lhs, Group rhs) {
    rhs._elements.insert(rhs._elements.begin(), make_shared(lhs));
    return rhs;
  }
};
// create a Group from 2 elements (not of type Group)
template <typename T, typename U>
  requires IsGrammarElement<T> &&
               (!std::same_as<std::decay_t<T>, Group>) && IsGrammarElement<U> &&
               (!std::same_as<std::decay_t<U>, Group>)
Group operator,(T &&lhs, U &&rhs) {
  return Group(std::forward<T>(lhs), std::forward<U>(rhs));
}

class UnorderedGroup final : public GrammarElement {
public:
  UnorderedGroup(UnorderedGroup &&) = default;
  UnorderedGroup(const UnorderedGroup &) = default;
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...) &&
            (!std::same_as<std::decay_t<Args>, Group> && ...)
  explicit UnorderedGroup(Args &&...args)
      : _elements{make_shared(std::forward<Args>(args))...} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  std::vector<std::shared_ptr<GrammarElement>> _elements;

  // concat 2 UnorderedGroups
  friend UnorderedGroup &&operator&(UnorderedGroup &&lhs,
                                    UnorderedGroup &&rhs) {
    for (auto &elem : rhs._elements)
      lhs._elements.emplace_back(std::move(elem));
    return std::move(lhs);
  }

  // append an element to an existing UnorderedGroup
  template <typename T>
    requires IsGrammarElement<T> &&
             (!std::same_as<std::decay_t<T>, UnorderedGroup>)
  friend UnorderedGroup &&operator&(UnorderedGroup &&lhs, T &&rhs) {
    lhs._elements.emplace_back(make_shared(std::forward<T>(rhs)));
    return std::move(lhs);
  }

  // prepend an element to an existing UnorderedGroup
  template <typename T>
    requires IsGrammarElement<T> &&
             (!std::same_as<std::decay_t<T>, UnorderedGroup>)
  friend UnorderedGroup operator&(T &&lhs, UnorderedGroup rhs) {
    rhs._elements.insert(rhs._elements.begin(), make_shared(lhs));
    return rhs;
  }
};
// create a UnorderedGroup from 2 elements (not of type UnorderedGroup)
template <typename T, typename U>
  requires IsGrammarElement<T> &&
           (!std::same_as<std::decay_t<T>, UnorderedGroup>) &&
           IsGrammarElement<U> &&
           (!std::same_as<std::decay_t<U>, UnorderedGroup>)
UnorderedGroup operator&(T &&lhs, U &&rhs) {
  return UnorderedGroup(std::forward<T>(lhs), std::forward<U>(rhs));
}
class Optional final : public GrammarElement {
public:
  Optional(Optional &&) = default;
  Optional(const Optional &) = default;
  template <typename T>
    requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, Optional>)
  explicit Optional(T &&element)
      : _element{make_shared(std::forward<T>(element))} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override {}

private:
  std::shared_ptr<GrammarElement> _element;
};
class Many final : public GrammarElement {
public:
  Many(Many &&) = default;
  Many(const Many &) = default;
  template <typename T>
    requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, Many>)
  explicit Many(T &&element)
      : _element{make_shared(std::forward<T>(element))} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override {}

private:
  std::shared_ptr<GrammarElement> _element;
};
class AtLeastOne final : public GrammarElement {
public:
  AtLeastOne(AtLeastOne &&) = default;
  AtLeastOne(const AtLeastOne &) = default;
  template <typename T>
    requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, AtLeastOne>)
  explicit AtLeastOne(T &&element)
      : _element{make_shared(std::forward<T>(element))} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override {}

private:
  std::shared_ptr<GrammarElement> _element;
};

class Repetition final : public GrammarElement {
public:
  Repetition(Repetition &&) = default;
  Repetition(const Repetition &) = default;

  template <typename T>
    requires IsGrammarElement<T>
  explicit Repetition(T &&element, size_t min, size_t max)
      : _element{make_shared(std::forward<T>(element))}, _min{min}, _max{max} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;
  void accept(Visitor &v) const override;

private:
  std::shared_ptr<GrammarElement> _element;
  size_t _min;
  size_t _max;
};

class PrioritizedChoice final : public GrammarElement {
public:
  PrioritizedChoice(PrioritizedChoice &&) = default;
  PrioritizedChoice(const PrioritizedChoice &) = default;

  explicit PrioritizedChoice(
      std::vector<std::shared_ptr<GrammarElement>> &&elements);
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...) &&
            (!std::same_as<PrioritizedChoice, std::decay_t<Args>> && ...)
  explicit PrioritizedChoice(Args &&...args)
      : _elements{make_shared(std::forward<Args>(args))...} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;

  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::vector<std::shared_ptr<GrammarElement>> _elements;

  // concat 2 PrioritizedChoices
  friend PrioritizedChoice &&operator|(PrioritizedChoice &&lhs,
                                       PrioritizedChoice &&rhs) {
    for (auto &elem : rhs._elements)
      lhs._elements.emplace_back(std::move(elem));
    return std::move(lhs);
  }
  // append an element to an existing PrioritizedChoice
  template <typename T>
    requires IsGrammarElement<T> &&
             (!std::same_as<std::decay_t<T>, PrioritizedChoice>)
  friend PrioritizedChoice &&operator|(PrioritizedChoice &&lhs, T &&rhs) {
    lhs._elements.emplace_back(make_shared(std::forward<T>(rhs)));
    return std::move(lhs);
  }

  // prepend an element to an existing PrioritizedChoice
  template <typename T>
    requires IsGrammarElement<T> &&
             (!std::same_as<std::decay_t<T>, PrioritizedChoice>)
  friend PrioritizedChoice operator|(T &&lhs, PrioritizedChoice rhs) {
    rhs._elements.insert(rhs._elements.begin(), make_shared(lhs));
    return rhs;
  }
};

// create a PrioritizedChoice from 2 elements (not of type PrioritizedChoice)
template <typename T, typename U>
  requires IsGrammarElement<T> &&
           (!std::same_as<std::decay_t<T>, PrioritizedChoice>) &&
           IsGrammarElement<U> &&
           (!std::same_as<std::decay_t<U>, PrioritizedChoice>)
PrioritizedChoice operator|(T &&lhs, U &&rhs) {
  return PrioritizedChoice(std::forward<T>(lhs), std::forward<U>(rhs));
}

class AndPredicate final : public GrammarElement {
public:
  AndPredicate(AndPredicate &&) = default;
  AndPredicate(const AndPredicate &) = default;

  explicit AndPredicate(std::shared_ptr<GrammarElement> element);
  template <typename T>
    requires IsGrammarElement<T> &&
             (!std::same_as<std::decay_t<T>, AndPredicate>)
  explicit AndPredicate(T &&element)
      : _element{make_shared(std::forward<T>(element))} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::shared_ptr<GrammarElement> _element;
};

class NotPredicate final : public GrammarElement {
public:
  NotPredicate(NotPredicate &&) = default;
  NotPredicate(const NotPredicate &) = default;
  explicit NotPredicate(std::shared_ptr<GrammarElement> element);
  template <typename T>
    requires IsGrammarElement<T> &&
             (!std::same_as<std::decay_t<T>, NotPredicate>)
  explicit NotPredicate(T &&element)
      : _element{make_shared(std::forward<T>(element))} {}

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;

private:
  std::shared_ptr<GrammarElement> _element;
};

class AnyCharacter final : public GrammarElement {
public:
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  void accept(Visitor &v) const override;
};

class CharacterClass final : public GrammarElement {
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

class Rule : public GrammarElement {
public:
  virtual ParseResult parse(std::string_view sv) const = 0;
  const std::string &name() const noexcept;

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;
  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;
  std::size_t parse_terminal(std::string_view sv) const override;

  bool execute(std::any &data, CstNode &node) const {
    return _action(data, node);
  }

protected:
  explicit Rule(std::string_view name, ContextProvider provider,
                std::function<bool(std::any &, CstNode &)> action);

private:
  friend class ParserRule;
  friend class TerminalRule;
  friend class DataTypeRule;

  std::string _name;
  ContextProvider _context_provider;
  std::shared_ptr<GrammarElement> _element;
  /// the action to be called when invocating make_ast
  std::function<bool(std::any &, CstNode &)> _action;
};

class TerminalRule final : public Rule {
public:
  ParseResult parse(std::string_view sv) const override;

  explicit TerminalRule(std::string_view name, ContextProvider provider,
                        std::function<bool(std::any &, CstNode &)> action,
                        DataType type);

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         Context &c) const override;

  std::size_t parse_hidden(std::string_view sv, CstNode &parent) const override;

  void accept(Visitor &v) const override;

  /// Initialize the rule with a list of elements
  /// @tparam ...Args
  /// @param ...args the list of elements
  /// @return a reference to the rule
  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  TerminalRule &operator()(Args &&...args) {
    _element = make_shared((std::forward<Args>(args), ...));
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

  DataType getType() const noexcept { return _type; }

private:
  DataType _type;
  enum class Kind {
    // a terminal mapped to a normal CstNode (non-hidden)
    Normal,
    // a terminal mapped to an hidden CstNode
    Hidden,
    // a terminal not mapped to a CstNode
    Ignored
  };

  Kind _kind = Kind::Normal;
};

class ParserRule final : public Rule {
public:
  explicit ParserRule(std::string_view name, ContextProvider provider,
                      std::function<bool(std::any &, CstNode &)> action);
  ParserRule(const ParserRule &) = delete;
  ParserRule(ParserRule &&) = delete;
  void accept(Visitor &v) const override;
  ParseResult parse(std::string_view sv) const override;

  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  ParserRule &operator()(Args &&...args) {
    _element = make_shared((std::forward<Args>(args), ...));
    return *this;
  }
};

class DataTypeRule final : public Rule {
public:
  explicit DataTypeRule(std::string_view name, ContextProvider provider,
                        std::function<bool(std::any &, CstNode &)> action,
                        DataType type);
  // DataTypeRule(const DataTypeRule &) = delete;
  // DataTypeRule(ParserRule &&) = delete;
  void accept(Visitor &v) const override;
  ParseResult parse(std::string_view sv) const override;

  template <typename... Args>
    requires(IsGrammarElement<Args> && ...)
  DataTypeRule &operator()(Args &&...args) {
    _element = make_shared((std::forward<Args>(args), ...));
    return *this;
  }

  DataType getType() const noexcept { return _type; }

private:
  DataType _type;
  // TODO add value_converter
};

class RuleCall final : public GrammarElement {
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

inline Keyword operator"" _kw(const char *str, std::size_t s) {
  return Keyword(std::string(str, s));
}

inline Keyword operator"" _ikw(const char *str, std::size_t s) {
  return Keyword(std::string(str, s), true);
}

inline Character operator"" _kw(char chr) { return Character(chr); }


template <typename T>
  requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, AndPredicate>)
inline AndPredicate operator&(T &&ope) {
  return AndPredicate(std::forward<T>(ope));
}
template <typename T>
  requires IsGrammarElement<T> && (!std::same_as<std::decay_t<T>, NotPredicate>)
inline NotPredicate operator!(T &&ope) {
  return NotPredicate(std::forward<T>(ope));
}

} // namespace pegium