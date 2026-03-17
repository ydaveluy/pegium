#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/references/Scope.hpp>
#include <pegium/references/ScopeProvider.hpp>

namespace pegium::references {
namespace {

struct LinkerNode : AstNode {
  string name;
};

struct LinkerReferrer : AstNode {
  reference<LinkerNode> node;
};

struct LinkerRoot : AstNode {
  vector<pointer<LinkerNode>> nodes;
  vector<pointer<LinkerReferrer>> referrers;
};

class BrokenScopeProvider final : public ScopeProvider {
public:
  std::shared_ptr<const Scope>
  getScope(const ReferenceInfo &context) const override {
    if (auto *referrer =
            dynamic_cast<LinkerReferrer *>(context.container);
        referrer != nullptr) {
      (void)referrer->node.get();
    }
    return std::make_shared<EmptyScope>();
  }
};

bool has_diagnostic_message(const workspace::Document &document,
                            std::string_view needle) {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<const parser::Parser> make_linker_parser() {
  auto parser = std::make_unique<test::FakeParser>();
  parser->callback = [](workspace::Document &document) {
    auto root = std::make_unique<LinkerRoot>();

    auto node = std::make_unique<LinkerNode>();
    node->name = "a";
    auto *nodePtr = node.get();
    root->nodes.push_back(std::move(node));
    nodePtr->setContainer<LinkerRoot, &LinkerRoot::nodes>(*root, 0);

    auto referrer = std::make_unique<LinkerReferrer>();
    auto *referrerPtr = referrer.get();
    referrerPtr->node.initialize<LinkerNode>(referrerPtr, "a", std::nullopt,
                                             false);
    root->referrers.push_back(std::move(referrer));
    referrerPtr->setContainer<LinkerRoot, &LinkerRoot::referrers>(*root, 0);

    document.parseResult.value = std::move(root);
    document.parseResult.references.push_back(
        ReferenceHandle::direct(&referrerPtr->node));
    document.references = document.parseResult.references;
  };
  return parser;
}

TEST(DefaultLinkerTest, DetectsCyclicReferenceResolution) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(
      *shared, "linker", {".link"}, {}, make_linker_parser());
  services->references.scopeProvider = std::make_unique<BrokenScopeProvider>();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("cyclic.link"), "linker",
      "node a\nreferrer a\n");
  ASSERT_NE(document, nullptr);

  auto *root = dynamic_cast<LinkerRoot *>(document->parseResult.value.get());
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->referrers.size(), 1u);

  const auto &reference = root->referrers.front()->node;
  EXPECT_TRUE(reference.hasError());
  EXPECT_EQ(reference.get(), nullptr);
  EXPECT_NE(reference.getErrorMessage().find("Cyclic reference resolution detected."),
            std::string::npos);
  EXPECT_TRUE(has_diagnostic_message(*document, "Unresolved reference: a"));
}

} // namespace
} // namespace pegium::references
