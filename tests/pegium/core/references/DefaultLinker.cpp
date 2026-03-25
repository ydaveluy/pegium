#include <gtest/gtest.h>

#include <pegium/core/CoreTestSupport.hpp>
#include <stdexcept>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>

namespace pegium::references {
namespace {

struct LinkerNode : AstNode {
  string name;
};

struct LinkerReferrer : AstNode {
  reference<LinkerNode> node;
};

struct LinkerMultiReferrer : AstNode {
  multi_reference<LinkerNode> nodes;
};

struct LinkerRoot : AstNode {
  vector<pointer<LinkerNode>> nodes;
  vector<pointer<LinkerReferrer>> referrers;
  vector<pointer<LinkerMultiReferrer>> multiReferrers;
};

template <typename TargetType, bool IsMulti = false>
struct TestReferenceAssignment final : grammar::Assignment {
  explicit TestReferenceAssignment(std::string_view feature) noexcept
      : feature(feature) {}

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }
  void execute(AstNode *, const CstNodeView &,
               const parser::ValueBuildContext &) const override {}
  [[nodiscard]] grammar::FeatureValue getValue(const AstNode *) const override {
    return {};
  }
  [[nodiscard]] const grammar::AbstractElement *
  getElement() const noexcept override {
    return nullptr;
  }
  [[nodiscard]] std::string_view getFeature() const noexcept override {
    return feature;
  }
  [[nodiscard]] bool isReference() const noexcept override { return true; }
  [[nodiscard]] bool isMultiReference() const noexcept override {
    return IsMulti;
  }
  [[nodiscard]] std::type_index getType() const noexcept override {
    return std::type_index(typeid(TargetType));
  }

  std::string_view feature;
};

class BrokenScopeProvider final : public ScopeProvider {
public:
  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const override {
    if (const auto *referrer =
            dynamic_cast<const LinkerReferrer *>(context.container);
        referrer != nullptr) {
      (void)referrer->node.get();
    }
    return nullptr;
  }

  bool visitScopeEntries(
      const ReferenceInfo &context,
      utils::function_ref<bool(const workspace::AstNodeDescription &)>)
      const override {
    (void)getScopeEntry(context);
    return true;
  }
};

class StaticScopeProvider final : public ScopeProvider {
public:
  explicit StaticScopeProvider(
      std::vector<workspace::AstNodeDescription> entries)
      : _entries(std::move(entries)) {}

  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const override {
    for (const auto &entry : _entries) {
      if (entry.name == context.referenceText) {
        return std::addressof(entry);
      }
    }
    return nullptr;
  }

private:
  bool visitScopeEntries(
      const ReferenceInfo &context,
      utils::function_ref<bool(const workspace::AstNodeDescription &)> visitor)
      const override {
    for (const auto &entry : _entries) {
      if (!context.referenceText.empty() &&
          entry.name != context.referenceText) {
        continue;
      }
      if (!visitor(entry)) {
        return false;
      }
    }
    return true;
  }

  std::vector<workspace::AstNodeDescription> _entries;
};

class EmptyScopeProvider final : public ScopeProvider {
public:
  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &) const override {
    return nullptr;
  }

  bool visitScopeEntries(
      const ReferenceInfo &,
      utils::function_ref<bool(const workspace::AstNodeDescription &)>)
      const override {
    return true;
  }
};

class ThrowingScopeProvider final : public ScopeProvider {
public:
  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &) const override {
    throw std::runtime_error("boom");
  }

  bool visitScopeEntries(
      const ReferenceInfo &,
      utils::function_ref<bool(const workspace::AstNodeDescription &)>)
      const override {
    throw std::runtime_error("boom");
  }
};

class DuplicateEntriesScopeProvider final : public ScopeProvider {
public:
  explicit DuplicateEntriesScopeProvider(
      std::vector<workspace::AstNodeDescription> entries)
      : _entries(std::move(entries)) {}

  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const override {
    for (const auto &entry : _entries) {
      if (entry.name == context.referenceText) {
        return std::addressof(entry);
      }
    }
    return nullptr;
  }

  bool visitScopeEntries(
      const ReferenceInfo &context,
      utils::function_ref<bool(const workspace::AstNodeDescription &)> visitor)
      const override {
    for (const auto &entry : _entries) {
      if (!context.referenceText.empty() &&
          entry.name != context.referenceText) {
        continue;
      }
      if (!visitor(entry)) {
        return false;
      }
    }
    return true;
  }

private:
  std::vector<workspace::AstNodeDescription> _entries;
};

struct DummyLiteral final : grammar::Literal {
  [[nodiscard]] bool isNullable() const noexcept override { return false; }

  [[nodiscard]] std::string_view getValue() const noexcept override {
    return "a";
  }

  [[nodiscard]] bool isCaseSensitive() const noexcept override { return true; }
};

struct TargetDocumentFixture {
  std::shared_ptr<workspace::Document> document;
  LinkerNode *target = nullptr;
};

struct ReferenceDocumentFixture {
  std::shared_ptr<workspace::Document> document;
  LinkerReferrer *referrer = nullptr;
};

struct MultiReferenceDocumentFixture {
  std::shared_ptr<workspace::Document> document;
  LinkerMultiReferrer *referrer = nullptr;
};

struct AttachedLinkerReferrerFixture {
  std::shared_ptr<workspace::Document> document;
  LinkerReferrer *referrer = nullptr;
};

TargetDocumentFixture make_target_document(pegium::SharedCoreServices &shared,
                                           std::string uri) {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(std::move(uri), "linker", "a"));
  shared.workspace.documents->addDocument(document);

  auto root = std::make_unique<LinkerRoot>();
  auto cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  static const DummyLiteral literal;
  CstBuilder builder(*cst);
  builder.leaf(0, 1, &literal);

  auto target = std::make_unique<LinkerNode>();
  target->name = "a";
  auto *targetPtr = target.get();
  targetPtr->setCstNode(cst->get(0));
  root->nodes.push_back(std::move(target));
  targetPtr->setContainer<LinkerRoot, &LinkerRoot::nodes>(*root, 0);

  document->parseResult.value = std::move(root);
  document->parseResult.cst = std::move(cst);
  return {.document = std::move(document), .target = targetPtr};
}

ReferenceDocumentFixture
make_reference_document(pegium::SharedCoreServices &shared,
                        const references::Linker &linker, std::string uri,
                        std::string refText = "a") {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(std::move(uri), "linker", refText));
  shared.workspace.documents->addDocument(document);

  auto root = std::make_unique<LinkerRoot>();
  auto cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  static const DummyLiteral literal;
  CstBuilder builder(*cst);
  builder.leaf(0, static_cast<TextOffset>(refText.size()), &literal);

  static const TestReferenceAssignment<LinkerNode> assignment("node");
  auto referrer = std::make_unique<LinkerReferrer>();
  auto *referrerPtr = referrer.get();
  referrerPtr->setCstNode(cst->get(0));
  referrerPtr->node.initialize(*referrerPtr, refText, cst->get(0), assignment,
                               linker);
  root->referrers.push_back(std::move(referrer));
  referrerPtr->setContainer<LinkerRoot, &LinkerRoot::referrers>(*root, 0);

  document->parseResult.value = std::move(root);
  document->parseResult.cst = std::move(cst);
  document->references.push_back(ReferenceHandle::direct(&referrerPtr->node));
  return {.document = std::move(document), .referrer = referrerPtr};
}

MultiReferenceDocumentFixture
make_multi_reference_document(pegium::SharedCoreServices &shared,
                              const references::Linker &linker, std::string uri,
                              std::string refText = "a") {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(std::move(uri), "linker", refText));
  shared.workspace.documents->addDocument(document);

  auto root = std::make_unique<LinkerRoot>();
  auto cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  static const DummyLiteral literal;
  CstBuilder builder(*cst);
  builder.leaf(0, static_cast<TextOffset>(refText.size()), &literal);

  static const TestReferenceAssignment<LinkerNode, true> assignment("nodes");
  auto referrer = std::make_unique<LinkerMultiReferrer>();
  auto *referrerPtr = referrer.get();
  referrerPtr->setCstNode(cst->get(0));
  referrerPtr->nodes.initialize(*referrerPtr, refText, cst->get(0), assignment,
                                linker);
  root->multiReferrers.push_back(std::move(referrer));
  referrerPtr->setContainer<LinkerRoot, &LinkerRoot::multiReferrers>(*root, 0);

  document->parseResult.value = std::move(root);
  document->parseResult.cst = std::move(cst);
  document->references.push_back(ReferenceHandle::direct(&referrerPtr->nodes));
  return {.document = std::move(document), .referrer = referrerPtr};
}

AttachedLinkerReferrerFixture
make_attached_referrer(pegium::SharedCoreServices &shared,
                       const references::Linker &linker, std::string uri,
                       std::string refText = "a") {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(std::move(uri), "linker", refText));
  shared.workspace.documents->addDocument(document);

  auto cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  static const DummyLiteral literal;
  CstBuilder builder(*cst);
  builder.leaf(0, static_cast<TextOffset>(refText.size()), &literal);

  static const TestReferenceAssignment<LinkerNode> assignment("node");
  auto referrer = std::make_unique<LinkerReferrer>();
  auto *referrerPtr = referrer.get();
  referrerPtr->setCstNode(cst->get(0));
  referrerPtr->node.initialize(*referrerPtr, refText, cst->get(0), assignment,
                               linker);

  document->parseResult.value = std::move(referrer);
  document->parseResult.cst = std::move(cst);
  document->references.push_back(ReferenceHandle::direct(&referrerPtr->node));
  return {.document = std::move(document), .referrer = referrerPtr};
}

bool has_diagnostic_message(const workspace::Document &document,
                            std::string_view needle) {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<const parser::Parser>
make_linker_parser(const Linker *&linkerRef) {
  auto parser = std::make_unique<test::FakeParser>();
  parser->callback = [&linkerRef](parser::ParseResult &result,
                                  std::string_view text) {
    static const TestReferenceAssignment<LinkerNode> assignment("node");
    static const DummyLiteral literal;
    assert(linkerRef != nullptr);
    auto root = std::make_unique<LinkerRoot>();
    auto cst = std::make_unique<RootCstNode>(text::TextSnapshot::copy(text));
    CstBuilder builder(*cst);
    builder.leaf(0, static_cast<TextOffset>(text.size()), &literal);
    root->setCstNode(cst->get(0));

    auto node = std::make_unique<LinkerNode>();
    node->name = "a";
    auto *nodePtr = node.get();
    nodePtr->setCstNode(cst->get(0));
    root->nodes.push_back(std::move(node));
    nodePtr->setContainer<LinkerRoot, &LinkerRoot::nodes>(*root, 0);

    auto referrer = std::make_unique<LinkerReferrer>();
    auto *referrerPtr = referrer.get();
    referrerPtr->setCstNode(cst->get(0));
    referrerPtr->node.initialize(*referrerPtr, "a", cst->get(0), assignment,
                                 *linkerRef);
    root->referrers.push_back(std::move(referrer));
    referrerPtr->setContainer<LinkerRoot, &LinkerRoot::referrers>(*root, 0);

    result.value = std::move(root);
    result.cst = std::move(cst);
    result.references.push_back(ReferenceHandle::direct(&referrerPtr->node));
  };
  return parser;
}

TEST(DefaultLinkerTest, DetectsCyclicReferenceResolution) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  const Linker *linkerRef = nullptr;
  auto services = test::make_uninstalled_core_services(
      *shared, "linker", {".link"}, {}, make_linker_parser(linkerRef));
  pegium::installDefaultCoreServices(*services);
  services->references.scopeProvider = std::make_unique<BrokenScopeProvider>();
  linkerRef = services->references.linker.get();
  shared->serviceRegistry->registerServices(std::move(services));

  auto document =
      test::open_and_build_document(*shared, test::make_file_uri("cyclic.link"),
                                    "linker", "node a\nreferrer a\n");
  ASSERT_NE(document, nullptr);

  auto *root = dynamic_cast<LinkerRoot *>(document->parseResult.value.get());
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->referrers.size(), 1u);

  const auto &reference = root->referrers.front()->node;
  EXPECT_TRUE(reference.hasError());
  EXPECT_EQ(reference.get(), nullptr);
  EXPECT_NE(
      reference.getErrorMessage().find("Cyclic reference resolution detected:"),
      std::string::npos);
  EXPECT_NE(reference.getErrorMessage().find("/referrers@0/node (symbol 'a')"),
            std::string::npos);
  EXPECT_TRUE(has_diagnostic_message(*document, "Unresolved reference: a"));
}

TEST(DefaultLinkerTest,
     ResolvesSyntheticReferenceInfoWithoutConcreteReference) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  auto fixture =
      make_target_document(*shared, test::make_file_uri("synthetic.link"));
  TestReferenceAssignment<LinkerNode> assignment("node");

  services->references.scopeProvider = std::make_unique<StaticScopeProvider>(
      std::vector<workspace::AstNodeDescription>{
          {.name = "a",
           .type = std::type_index(typeid(LinkerNode)),
           .documentId = fixture.document->id,
           .symbolId = fixture.document->makeSymbolId(*fixture.target)}});

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  auto refFixture = make_attached_referrer(
      *shared, *linker, test::make_file_uri("synthetic-referrer.link"), "a");
  const ReferenceInfo info{refFixture.referrer, "a", assignment};

  const auto candidate = linker->getCandidate(info);
  ASSERT_TRUE(std::holds_alternative<workspace::AstNodeDescription>(candidate));
  EXPECT_EQ(std::get<workspace::AstNodeDescription>(candidate).name, "a");

  const auto candidates = linker->getCandidates(info);
  ASSERT_TRUE(
      std::holds_alternative<std::vector<workspace::AstNodeDescription>>(
          candidates));
  ASSERT_EQ(
      std::get<std::vector<workspace::AstNodeDescription>>(candidates).size(),
      1u);
  EXPECT_EQ(std::get<std::vector<workspace::AstNodeDescription>>(candidates)
                .front()
                .name,
            "a");
}

TEST(DefaultLinkerTest, UnresolvedCandidateMessageIncludesReferenceType) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  services->references.scopeProvider = std::make_unique<EmptyScopeProvider>();

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  static const TestReferenceAssignment<LinkerNode> assignment("node");
  auto refFixture = make_attached_referrer(
      *shared, *linker, test::make_file_uri("missing-referrer.link"),
      "missing");
  refFixture.document->state = workspace::DocumentState::ComputedScopes;
  const ReferenceInfo info{refFixture.referrer, "missing", assignment};

  const auto candidate = linker->getCandidate(info);
  ASSERT_TRUE(std::holds_alternative<workspace::LinkingError>(candidate));
  EXPECT_EQ(std::get<workspace::LinkingError>(candidate).message,
            "Could not resolve reference to LinkerNode named 'missing'.");
}

TEST(DefaultLinkerTest, GetCandidatesDeduplicatesDescriptionsByNodeKey) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  auto fixture =
      make_target_document(*shared, test::make_file_uri("duplicates.link"));

  const auto symbolId = fixture.document->makeSymbolId(*fixture.target);
  services->references.scopeProvider =
      std::make_unique<DuplicateEntriesScopeProvider>(
          std::vector<workspace::AstNodeDescription>{
              {.name = "a",
               .type = std::type_index(typeid(LinkerNode)),
               .documentId = fixture.document->id,
               .symbolId = symbolId},
              {.name = "a",
               .type = std::type_index(typeid(LinkerNode)),
               .documentId = fixture.document->id,
               .symbolId = symbolId}});

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  static const TestReferenceAssignment<LinkerNode> assignment("node");
  auto refFixture = make_attached_referrer(
      *shared, *linker, test::make_file_uri("duplicate-referrer.link"), "a");
  const ReferenceInfo info{refFixture.referrer, "a", assignment};

  const auto candidates = linker->getCandidates(info);
  ASSERT_TRUE(
      std::holds_alternative<std::vector<workspace::AstNodeDescription>>(
          candidates));
  EXPECT_EQ(
      std::get<std::vector<workspace::AstNodeDescription>>(candidates).size(),
      1u);
}

TEST(DefaultLinkerTest,
     RetryableResolutionBeforeComputedScopesDoesNotFreezeReferenceError) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto recordingSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = recordingSink;
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  services->references.scopeProvider = std::make_unique<EmptyScopeProvider>();

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  auto fixture = make_reference_document(
      *shared, *linker, test::make_file_uri("retryable.link"), "missing");
  fixture.document->state = workspace::DocumentState::Parsed;

  EXPECT_EQ(fixture.referrer->node.get(), nullptr);
  EXPECT_EQ(fixture.referrer->node.state(), ReferenceState::Unresolved);
  EXPECT_FALSE(fixture.referrer->node.hasError());
  EXPECT_TRUE(fixture.referrer->node.getErrorMessage().empty());

  fixture.document->state = workspace::DocumentState::ComputedScopes;

  EXPECT_EQ(fixture.referrer->node.get(), nullptr);
  EXPECT_EQ(fixture.referrer->node.state(), ReferenceState::Error);
  EXPECT_TRUE(fixture.referrer->node.hasError());
  EXPECT_EQ(fixture.referrer->node.getErrorMessage(),
            "Could not resolve reference to LinkerNode named 'missing'.");
  ASSERT_TRUE(recordingSink->waitForCount(1));
  const auto observation = recordingSink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::ReferenceResolutionProblem);
  EXPECT_EQ(observation->uri, fixture.document->uri);
  EXPECT_EQ(observation->state, workspace::DocumentState::Parsed);
}

TEST(DefaultLinkerTest, LinkCatchesScopeProviderExceptionsWithoutThrowing) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto recordingSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = recordingSink;
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  services->references.scopeProvider =
      std::make_unique<ThrowingScopeProvider>();

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  auto fixture = make_multi_reference_document(
      *shared, *linker, test::make_file_uri("throwing.link"), "missing");
  fixture.document->state = workspace::DocumentState::ComputedScopes;

  EXPECT_NO_THROW(linker->link(*fixture.document, {}));
  EXPECT_TRUE(fixture.referrer->nodes.hasError());
  EXPECT_EQ(fixture.referrer->nodes.getErrorMessage(),
            "An error occurred while resolving reference to 'missing': boom");
  ASSERT_TRUE(recordingSink->waitForCount(1));
  const auto observation = recordingSink->lastObservation();
  ASSERT_TRUE(observation.has_value());
  EXPECT_EQ(observation->code,
            observability::ObservationCode::ReferenceResolutionProblem);
  EXPECT_NE(observation->message.find("boom"), std::string::npos);
}

TEST(DefaultLinkerTest, InitializedReferenceResolvesAfterMoveConstruction) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  auto fixture =
      make_target_document(*shared, test::make_file_uri("move.link"));
  const auto documentId = fixture.document->id;
  const auto targetPtr = fixture.target;

  services->references.scopeProvider = std::make_unique<StaticScopeProvider>(
      std::vector<workspace::AstNodeDescription>{
          {.name = "a",
           .type = std::type_index(typeid(LinkerNode)),
           .documentId = documentId,
           .symbolId = fixture.document->makeSymbolId(*targetPtr)}});

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  static const TestReferenceAssignment<LinkerNode> assignment("node");
  auto refFixture = make_attached_referrer(
      *shared, *linker, test::make_file_uri("move-referrer.link"), "a");
  Reference<LinkerNode> reference;
  reference.initialize(*refFixture.referrer, "a",
                       refFixture.referrer->getCstNode(), assignment, *linker);

  auto moved = std::move(reference);
  const auto *resolved = moved.get();
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved, targetPtr);
  EXPECT_EQ(resolved->name, "a");
}

TEST(DefaultLinkerTest, ReinitializingReferenceResetsCachedResolution) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "linker");
  pegium::installDefaultCoreServices(*services);
  auto fixture =
      make_target_document(*shared, test::make_file_uri("reinitialize.link"));
  const auto documentId = fixture.document->id;
  const auto firstPtr = fixture.target;

  auto second = std::make_unique<LinkerNode>();
  second->name = "b";
  auto *secondPtr = second.get();
  static const DummyLiteral literal;
  CstBuilder builder(*fixture.document->parseResult.cst);
  builder.leaf(0, 1, &literal);
  secondPtr->setCstNode(fixture.document->parseResult.cst->get(1));
  auto *root =
      static_cast<LinkerRoot *>(fixture.document->parseResult.value.get());
  root->nodes.push_back(std::move(second));
  secondPtr->setContainer<LinkerRoot, &LinkerRoot::nodes>(*root, 1);

  services->references.scopeProvider = std::make_unique<StaticScopeProvider>(
      std::vector<workspace::AstNodeDescription>{
          {.name = "a",
           .type = std::type_index(typeid(LinkerNode)),
           .documentId = documentId,
           .symbolId = fixture.document->makeSymbolId(*firstPtr)},
          {.name = "b",
           .type = std::type_index(typeid(LinkerNode)),
           .documentId = documentId,
           .symbolId = fixture.document->makeSymbolId(*secondPtr)}});

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  static const TestReferenceAssignment<LinkerNode> assignment("node");
  auto refFixture = make_attached_referrer(
      *shared, *linker, test::make_file_uri("reinitialize-referrer.link"), "a");
  Reference<LinkerNode> reference;
  reference.initialize(*refFixture.referrer, "a",
                       refFixture.referrer->getCstNode(), assignment, *linker);

  ASSERT_EQ(reference.get(), firstPtr);
  ASSERT_TRUE(reference.isResolved());

  reference.initialize(*refFixture.referrer, "b",
                       refFixture.referrer->getCstNode(), assignment, *linker);

  EXPECT_EQ(reference.state(), ReferenceState::Unresolved);
  EXPECT_EQ(reference.get(), secondPtr);
  EXPECT_EQ(reference.get()->name, "b");
}

} // namespace
} // namespace pegium::references
