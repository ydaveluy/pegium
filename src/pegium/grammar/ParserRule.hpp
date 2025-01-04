#pragma once
#include <iostream>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/IAction.hpp>
#include <pegium/grammar/IAssignment.hpp>
#include <string_view>

namespace pegium::grammar {

template <typename T>
// requires std::derived_from<T, AstNode>
struct ParserRule final : AbstractRule {
  using type = T;
  ParserRule(std::string_view name = "", std::string_view description = "")
      : AbstractRule{name, description} {}
  ParserRule(const ParserRule &) = delete;
  ParserRule &operator=(const ParserRule &) = delete;

  std::any getAnyValue(const CstNode &node) const override {

    if constexpr (is_shared_ptr<T>::value) {
      return std::static_pointer_cast<AstNode>(getValue(node));
    } else {
      return std::static_pointer_cast<AstNode>(
          std::make_shared<T>(getValue(node)));
    }
  }

  T getValue(const CstNode &node) const {

    if constexpr (is_shared_ptr<T>::value) {
      using U = typename T::element_type;
      T value = std::make_shared<U>();
      for (auto it = node.begin(); it != node.end(); ++it) {
        if (it->grammarSource) {
          switch (it->grammarSource->getKind()) {
          case pegium::grammar::GrammarElementKind::Assignment:
            static_cast<const IAssignment *>(it->grammarSource)
                ->execute(value.get(), *it);
            it.prune();
            break;
          case pegium::grammar::GrammarElementKind::Action:
            value = std::dynamic_pointer_cast<U>(
                static_cast<const IAction *>(it->grammarSource)
                    ->execute(std::static_pointer_cast<AstNode>(value)));
            it.prune();
            break;
          case pegium::grammar::GrammarElementKind::ParserRule:
            value = std::dynamic_pointer_cast<U>(
                std::any_cast<std::shared_ptr<AstNode>>(
                    static_cast<const IRule *>(it->grammarSource)
                        ->getAnyValue(*it)));
            it.prune();
            break;
          default:
            break;
          }
        }
      }
      return value;
    } else {
      T value;

      // std::cout << "process root node '" << node << "'";
      for (auto it = node.begin(); it != node.end(); ++it) {
        if (it->grammarSource) {
          switch (it->grammarSource->getKind()) {
          case pegium::grammar::GrammarElementKind::Assignment:
            static_cast<const IAssignment *>(it->grammarSource)
                ->execute(&value, *it);
            it.prune();
            break;
          default:
            break;
          }
          /*if (auto *assignment =
                  dynamic_cast<const IAssignment *>(it->grammarSource)) {
            assignment->execute(&value, *it);
            it.prune();
          } else if (auto *rule =
                         dynamic_cast<const IRule *>(it->grammarSource)) {

            auto value = rule->getAnyValue(*it);
            if (auto ptr = std::any_cast<std::shared_ptr<AstNode>>(&value)) {
              if (auto asT = std::dynamic_pointer_cast<T>(*ptr)) {
                value = asT;
              } else {
                throw std::logic_error("The return type of the parser rule is "
                                       "not a base type of the rule call.");
              }
            }
          }*/
        }
      }
      return value;
    }
  }
  pegium::GenericParseResult parseGeneric(
      std::string_view text,
      std::unique_ptr<pegium::grammar::IContext> context) const override {
    auto result = parse(text, std::move(context));
    return {.root_node = result.root_node};
  }
  pegium::ParseResult<T>
  parse(std::string_view text,
        std::unique_ptr<pegium::grammar::IContext> context) const {
    pegium::ParseResult<T> result;
    result.root_node = std::make_shared<RootCstNode>();
    result.root_node->fullText = text;
    std::string_view sv = result.root_node->fullText;
    result.root_node->text = result.root_node->fullText;
    // auto c = _parser->createContext();

    auto i = context->skipHiddenNodes(sv, *result.root_node);
    auto skipped = result.root_node->content.size();
    result.len = i + parse_rule({sv.data() + i, sv.size() - i},
                                *result.root_node, *context);

    // std::cout << *result.root_node << std::endl;
    result.ret = result.len == sv.size();
    if (result.ret) {
      result.value = getValue(result.root_node->content.at(skipped));
    }

    return result;
  }
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const override {
    auto size = parent.content.size();
    auto &node = parent.content.emplace_back();

    auto i = element->parse_rule(sv, node, c);
    if (fail(i)) {
      parent.content.resize(size);
      return PARSE_ERROR;
    }
    node.text = {sv.data(), i};
    node.grammarSource = this;

    return i;
  }
  using AbstractRule::operator=;

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::ParserRule;
  }

private:
  template <typename U> struct is_shared_ptr : std::false_type {};

  template <typename U>
  struct is_shared_ptr<std::shared_ptr<U>> : std::true_type {};
};
} // namespace pegium::grammar