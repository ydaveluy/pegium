#include <gtest/gtest.h>

#include <pegium/syntax-tree/CstBuilder.hpp>
#include <pegium/syntax-tree/Json.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace {

struct DummyGrammarElement final : pegium::grammar::AbstractElement {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  void print(std::ostream &os) const override { os << "dummy\"\\\n"; }
};

} // namespace

TEST(SyntaxTreeTest, CstNodeJsonAndStreamIncludeHiddenAndNullGrammar) {
  pegium::CstNode node{
      .begin = 1,
      .end = 4,
      .grammarElement = nullptr,
      .nextSiblingId = pegium::kNoNode,
      .isLeaf = true,
      .isHidden = true,
  };

  const std::string json = pegium::toJson(node);
  EXPECT_NE(json.find("\"begin\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"end\": 4"), std::string::npos);
  EXPECT_NE(json.find("\"grammarSource\": null"), std::string::npos);
  EXPECT_NE(json.find("\"hidden\": true"), std::string::npos);

  std::ostringstream oss;
  oss << node;
  EXPECT_EQ(oss.str(), json);
}

TEST(SyntaxTreeTest, CstNodeJsonEscapesGrammarSource) {
  static const DummyGrammarElement grammarElement{};
  pegium::CstNode node{
      .begin = 0,
      .end = 1,
      .grammarElement = &grammarElement,
      .nextSiblingId = pegium::kNoNode,
      .isLeaf = true,
      .isHidden = false,
  };

  const std::string json = pegium::toJson(node);
  EXPECT_NE(json.find("\"grammarSource\": "), std::string::npos) << json;
  EXPECT_NE(json.find("\"grammarSource\": \"dummy"), std::string::npos) << json;
  EXPECT_NE(json.find("\\n\""), std::string::npos) << json;
}

TEST(SyntaxTreeTest, RootJsonIncludesNestedContentAndHiddenChildren) {
  pegium::CstBuilder builder{"ab"};
  const char *begin = builder.input_begin();
DummyGrammarElement ge;
  builder.enter();
  builder.leaf(begin, begin + 1, &ge, false);
  builder.leaf(begin + 1, begin + 2, &ge, true);
  builder.exit(begin, begin + 2, &ge);

  auto root = builder.finalize();
  const std::string json = pegium::toJson(*root);

  EXPECT_NE(json.find("\"fullText\": \"ab\""), std::string::npos);
  EXPECT_NE(json.find("\"content\": ["), std::string::npos);
  EXPECT_NE(json.find("\"hidden\": true"), std::string::npos);

  std::vector<pegium::CstNodeView> topLevelNodes;
  for (const auto &node : *root) {
    topLevelNodes.push_back(node);
  }
  ASSERT_EQ(topLevelNodes.size(), 1u);
  EXPECT_FALSE(topLevelNodes.front().isLeaf());
}

TEST(SyntaxTreeTest, RootJsonEscapesFullTextCharacters) {
  std::string input = "\"\\\b\f\n\r\t";
  input.push_back('\x01');

  pegium::RootCstNode root{input};
  const std::string json = pegium::toJson(root);

  EXPECT_NE(json.find("\\\""), std::string::npos);
  EXPECT_NE(json.find("\\\\"), std::string::npos);
  EXPECT_NE(json.find("\\b"), std::string::npos);
  EXPECT_NE(json.find("\\f"), std::string::npos);
  EXPECT_NE(json.find("\\n"), std::string::npos);
  EXPECT_NE(json.find("\\r"), std::string::npos);
  EXPECT_NE(json.find("\\t"), std::string::npos);
  EXPECT_NE(json.find("\\u0001"), std::string::npos);
}

TEST(SyntaxTreeTest, RootJsonSeparatesMultipleTopLevelNodesWithComma) {
  pegium::CstBuilder builder{"ab"};
  const char *begin = builder.input_begin();
DummyGrammarElement ge;
  builder.leaf(begin, begin + 1, &ge, false);
  builder.leaf(begin + 1, begin + 2, &ge, false);

  auto root = builder.finalize();
  const std::string json = pegium::toJson(*root);

  EXPECT_NE(json.find(",\n    {"), std::string::npos);
}
