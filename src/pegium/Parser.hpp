

#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <pegium/IParser.hpp>
#include <pegium/grammar.hpp>
#include <pegium/syntax-tree.hpp>
#include <string>
#include <string_view>

namespace pegium {

class Parser : public IParser {
public:
  ParseResult parse(const std::string &input) const override { return {}; }
  ParseResult parse(const std::string &name, std::string_view text) const {
    auto c = createContext();
    auto result = _rules.at(name)->parse(text, c);

    // result.value = getValue(*result.root_node);
    return result;
    return {};
  }
  ~Parser() noexcept override = default;

public:
  template <typename T>
    requires std::derived_from<T, AstNode>
  grammar::ParserRule &rule(std::string name) {
    auto rule = std::make_shared<grammar::ParserRule>(
        /*name, [this] { return this->createContext(); }, make_converter<T>()*/);

    _rules[name] = rule;
    return *rule.get();
  }

  template <typename T = std::string>
    requires(!std::derived_from<T, AstNode>)
  grammar::DataTypeRule &rule(std::string name) {
    auto rule = std::make_shared<grammar::DataTypeRule>(
        /*name, [this] { return this->createContext(); }, make_converter<T>()*/);

    _rules[name] = rule;
    return *rule.get();
  }

  template <typename T = std::string>
    requires(!std::derived_from<T, AstNode>)
  grammar::TerminalRule &terminal(std::string name) {

    auto rule = std::make_shared<grammar::TerminalRule>(
        /*name, [this] { return this->createContext(); }, make_converter<T>()*/);

    _rules[name] = rule;
    return *rule.get();
  }

  /// Call an other rule
  /// @param name the rule name
  /// @return the call element
 grammar::RuleCall call(const std::string &name) {
    return grammar::RuleCall{_rules[name]};
  }

 
private:
  grammar::Context createContext() const {

    std::vector<const grammar::Rule *> hiddens;
    for (auto &[_, def] : _rules) {
      if (auto *terminal = dynamic_cast<grammar::TerminalRule *>(def.get())) {
        if (terminal->hidden())
          hiddens.push_back(terminal);
      }
    }

    return grammar::Context{std::move(hiddens)};
  }

  template <typename T>
  std::function<bool(std::any &, CstNode &)> make_converter() const {
    return [](std::any &value, CstNode &node) {
      if constexpr (std::is_base_of_v<AstNode, T>) {
        value = std::static_pointer_cast<AstNode>(std::make_shared<T>());
        return true;
      } else {
        std::string result;
        for (const auto &n : node) {
          if (n.isLeaf && !n.hidden) {
            result += n.text;
          }
        }
        value = result;
        return false;
      }
    };
  }
  std::map<std::string, std::shared_ptr<grammar::Rule>, std::less<>> _rules;
};

} // namespace pegium