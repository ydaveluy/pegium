#pragma once
#include <iostream>
#include <pegium/grammar/Action.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/Action.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/IParser.hpp>
#include <string_view>
#include <vector>

namespace pegium::parser {

template <typename T>
  requires std::derived_from<T, AstNode>
struct ParserRule final : AbstractRule {
  using type = T;
  using AbstractRule::AbstractRule;
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }

  std::any getAnyValue(const CstNode &node) const override {
    // always return an std::shared_ptr<AstNode>
    return std::static_pointer_cast<AstNode>(getValue(node));
  }

  std::shared_ptr<T> getValue(const CstNode &node) const {
    std::shared_ptr<T> current;
    std::vector<std::pair<const grammar::Assignment *, const CstNode *>>
        assignments;
    for (const auto &it : node.content) {
      if (it.grammarSource) {
        switch (it.grammarSource->getKind()) {
        case ElementKind::Assignment:
          // store the assignment
          assignments.emplace_back(
              static_cast<const grammar::Assignment *>(it.grammarSource), &it);
          break;
        case ElementKind::New:
          // instanciate a new value
          current = std::static_pointer_cast<T>(
              static_cast<const grammar::Action *>(it.grammarSource)
                  ->execute(std::static_pointer_cast<AstNode>(current)));
          break;
        case ElementKind::Init:
          // create the current value if not already defined
          if (!current)
            current = std::make_shared<T>();
          // and apply all assignments
          for (const auto &[assignment, node] : assignments)
            assignment->execute(current.get(), *node);
          assignments.clear();
          // instanciate a new value and initilize the feature with current
          // value
          current = std::static_pointer_cast<T>(
              static_cast<const grammar::Action *>(it.grammarSource)
                  ->execute(std::static_pointer_cast<AstNode>(current)));
          break;
        case ElementKind::ParserRule:
          // create a new value from a parser rule
          current = std::static_pointer_cast<T>(
              std::any_cast<std::shared_ptr<AstNode>>(
                  static_cast<const grammar::Rule *>(it.grammarSource)
                      ->getAnyValue(it)));
          break;
        }
      }
    }
    // create the current value if not already defined
    if (!current)
      current = std::make_shared<T>();
    // and apply all assignments
    for (const auto &[assignment, node] : assignments)
      assignment->execute(current.get(), *node);
    return current;
  }
  GenericParseResult parseGeneric(std::string_view text,
                                  std::unique_ptr<IContext> context) const
  /*override */ {
    auto result = parse(text, std::move(context));
    return {.root_node = result.root_node};
  }
  ParseResult<std::shared_ptr<T>>
  parse(std::string_view text, std::unique_ptr<IContext> context) const {
    ParseResult<std::shared_ptr<T>> result;
    result.root_node = std::make_shared<RootCstNode>();
    result.root_node->fullText = text;
    std::string_view sv = result.root_node->fullText;
    result.root_node->text = result.root_node->fullText;

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
                         IContext &c) const {
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
};
} // namespace pegium::parser