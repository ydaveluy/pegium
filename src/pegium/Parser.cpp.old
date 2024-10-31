#include <pegium/Parser.hpp>

namespace pegium {
Context Parser::createContext() const {

  struct HiddenVisitor : public GrammarElement::Visitor {

    void visit(const TerminalRule &rule) override { _hidden = rule.hidden(); }
    static bool isHidden(const GrammarElement &elem) {
      HiddenVisitor v;
      elem.accept(v);
      return v._hidden;
    }

  private:
    bool _hidden = false;
  };

  std::vector<std::shared_ptr<GrammarElement>> hiddenRules;
  for (auto &[_, def] : _rules) {
    if (HiddenVisitor::isHidden(*def)) {
      hiddenRules.emplace_back(std::make_shared<RuleCall>(def));
    }
  }
  if (hiddenRules.empty())
    return Context{};

  if (hiddenRules.size() == 1) {
    return Context{std::move(hiddenRules.front())};
  }
  return Context{std::make_shared<PrioritizedChoice>(std::move(hiddenRules))};
}

std::any getValue(std::vector<std::unique_ptr<CstNode>> &node);

struct AstBuilder : public GrammarElement::Visitor {
  static bool build(const GrammarElement &element, CstNode &node,
                    std::any &result) {
    AstBuilder builder{result, node};
    element.accept(builder);
    return builder._prune;
  }
  void visit(const Action &rule) override {
    // execute the action
    rule.execute(_result);
  }
  void visit(const ParserRule &rule) override {

    // create the object
    rule.execute(_result, _node);
  }
  void visit(const TerminalRule &rule) override {

    //
  }
  void visit(const Assignment &assignment) override;

private:
  std::any &_result;
  CstNode &_node;
  bool _prune = false;
  AstBuilder(std::any &result, CstNode &node) : _result{result}, _node{node} {}
  inline void prune() { _prune = true; }
};

struct RootAstBuilder : public GrammarElement::Visitor {
  static std::any build(const GrammarElement &element, CstNode &node) {
    std::any result;
    RootAstBuilder builder{result, node};
    element.accept(builder);
    if (!result.has_value()) {
      std::string text;
      for (const auto &n : node)
        if (n.isLeaf && !n.hidden)
          text += n.text;
      result = text;
    }
    return result;
  }

 // void visit(const Assignment &assignment) override {}
  void visit(const ParserRule &rule) override {
    for (auto it = _node.begin(); it != _node.end(); ++it) {
      if (AstBuilder::build(*it->grammarSource, *it, _result))
        it.prune();
    }
  }

  void visit(const DataTypeRule &rule) override {

    std::string text;
    for (const auto &node : _node) {
      if (node.isLeaf && !node.hidden)
        text += node.text;
    }
    /*if (rule.getValueConverter()) {
      _result = rule.getValueConverter()(text);
    } else*/
    _result = text;
  }
  void visit(const TerminalRule &rule) override {
    std::string text;
    for (const auto &node : _node) {
      if (node.isLeaf && !node.hidden)
        text += node.text;
    }
    /*if (rule.getValueConverter()) {
      _result = rule.getValueConverter()(text);
    } else*/
    _result = text;
  }

private:
  RootAstBuilder(std::any &result, CstNode &node)
      : _result{result}, _node{node} {}
  std::any &_result;
  CstNode &_node;
};

void AstBuilder::visit(const Assignment &assignment) {

  // build the value from all nested nodes
  /*for (auto &node : _node.content)
    for (auto it = node.begin(); it != node.end(); ++it) {
    }*/
  // assign the computed value
  assignment.getFeature().assign(
      _result, RootAstBuilder::build(*_node.content.front().grammarSource,
                                     _node.content.front()));
  // prune the current node (the children are already consumed by the
  // assignment)
  prune();
}

std::any getValue(CstNode &node) {
  return RootAstBuilder::build(*node.grammarSource, node);
}

ParseResult Parser::parse(const std::string &name,
                          std::string_view text) const {
  auto result = _rules.at(name)->parse(text);

  result.value = getValue(*result.root_node);
  return result;
}

ParseResult Parser::parse(const std::string &input) const {
  // TODO get entry rule
  // return parse(entryRuleName, input);
  return {};
}

} // namespace pegium