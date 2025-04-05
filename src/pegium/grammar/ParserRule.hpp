#pragma once
#include <iostream>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/IAction.hpp>
#include <pegium/grammar/IAssignment.hpp>
#include <string_view>

namespace pegium::grammar {

template <typename T>
  requires std::derived_from<T, AstNode>
struct ParserRule final : AbstractRule {
  using type = T;

  using AbstractRule::AbstractRule;

  /*ParserRule(std::string_view name = "", std::string_view description = "")
      : AbstractRule{name, description} {}*/
  ParserRule(const ParserRule &) = delete;
  ParserRule &operator=(const ParserRule &) = delete;

  std::any getAnyValue(const CstNode &node) const override {

    return std::static_pointer_cast<AstNode>(getValue(node));
  }

  std::shared_ptr<T> getValue(const CstNode &node) const {
    std::shared_ptr<T> value;
    std::vector<std::pair<const IAssignment *, const CstNode *>> assignments;
    for (auto &it : node.content) {
      if (it.grammarSource) {
        switch (it.grammarSource->getKind()) {
        case pegium::grammar::GrammarElementKind::Assignment: {
          const auto *assignment =
              static_cast<const IAssignment *>(it.grammarSource);
          if (value) // TODO only execute at end or before assignment/action ?
            assignment->execute(value.get(), it);
          else
            assignments.emplace_back(assignment, &it);
          break;
        }
        case pegium::grammar::GrammarElementKind::Action:
          if (!value)
            value = std::make_shared<T>();
          for (auto &assignment : assignments)
            assignment.first->execute(value.get(), *assignment.second);

          value = std::static_pointer_cast<T>(
              static_cast<const IAction *>(it.grammarSource)
                  ->execute(std::static_pointer_cast<AstNode>(value)));
          break;
        case pegium::grammar::GrammarElementKind::ParserRule:
          value = std::static_pointer_cast<T>(
              std::any_cast<std::shared_ptr<AstNode>>(
                  static_cast<const IRule *>(it.grammarSource)
                      ->getAnyValue(it)));
          // apply leading assignments
          for (auto &assignment : assignments)
            assignment.first->execute(value.get(), *assignment.second);
          break;
        default:
          break;
        }
      }
    }
    if (!value)
      value = std::make_shared<T>();
    for (auto &assignment : assignments)
      assignment.first->execute(value.get(), *assignment.second);
    return value;
  }
  pegium::GenericParseResult parseGeneric(
      std::string_view text,
      std::unique_ptr<pegium::grammar::IContext> context) const override {
    auto result = parse(text, std::move(context));
    return {.root_node = result.root_node};
  }
  pegium::ParseResult<std::shared_ptr<T>>
  parse(std::string_view text,
        std::unique_ptr<pegium::grammar::IContext> context) const {
    pegium::ParseResult<std::shared_ptr<T>> result;
    result.root_node = std::make_shared<RootCstNode>();
    result.root_node->fullText = text;
    std::string_view sv = result.root_node->fullText;
    result.root_node->text = result.root_node->fullText;
    // auto c = _parser->createContext();

    auto i = context->skipHiddenNodes(sv, *result.root_node);
    auto skipped = result.root_node->content.size();

    auto match = parse_rule({i.offset, sv.end()}, *result.root_node, *context);

    result.len = match.offset - sv.begin();

    // std::cout << *result.root_node << std::endl;
    result.ret = match;
    if (result.ret) {
      result.value = getValue(result.root_node->content.at(skipped));
    }

    return result;
  }
  MatchResult parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const override {
    assert(element);
    CstNode node;
    auto i = element->parse_rule(sv, node, c);
    if (i) {
      node.grammarSource = this;
      node.text = {sv.begin(), i.offset};
      parent.content.emplace_back(std::move(node));
    }
    return i;
  }
  using AbstractRule::operator=;

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::ParserRule;
  }

private:
  /*template <typename U> struct is_shared_ptr : std::false_type {};

  template <typename U>
  struct is_shared_ptr<std::shared_ptr<U>> : std::true_type {};*/
};
} // namespace pegium::grammar