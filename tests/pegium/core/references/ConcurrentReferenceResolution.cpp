#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>

namespace pegium::references {
namespace {

struct ConcurrentNode : AstNode {
  string name;
};

struct ConcurrentReferrer : AstNode {
  reference<ConcurrentNode> node;
};

struct ConcurrentMultiReferrer : AstNode {
  multi_reference<ConcurrentNode> nodes;
};

struct ConcurrentRoot : AstNode {
  vector<pointer<ConcurrentNode>> nodes;
  vector<pointer<ConcurrentReferrer>> referrers;
  vector<pointer<ConcurrentMultiReferrer>> multiReferrers;
};

template <typename OwnerType, typename TargetType, bool IsMulti = false>
struct TestRefAssignment final : grammar::Assignment {
  explicit TestRefAssignment(std::string_view feature) noexcept
      : feature(feature) {}

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }
  void execute(AstNode *, const CstNodeView &,
               const parser::ValueBuildContext &) const override {}
  [[nodiscard]] grammar::FeatureValue
  getValue(const AstNode *) const override {
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

struct DummyLit final : grammar::Literal {
  [[nodiscard]] bool isNullable() const noexcept override { return false; }
  [[nodiscard]] std::string_view getValue() const noexcept override {
    return "a";
  }
  [[nodiscard]] bool isCaseSensitive() const noexcept override { return true; }
};

/// Scope provider that records call counts and sleeps to widen race window.
class CountingDelayingScopeProvider final : public ScopeProvider {
public:
  CountingDelayingScopeProvider(
      std::vector<workspace::AstNodeDescription> entries,
      std::chrono::microseconds delay)
      : _entries(std::move(entries)), _delay(delay) {}

  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &context) const override {
    _scopeEntryCalls.fetch_add(1, std::memory_order_relaxed);
    if (_delay.count() > 0) {
      std::this_thread::sleep_for(_delay);
    }
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
    _visitCalls.fetch_add(1, std::memory_order_relaxed);
    if (_delay.count() > 0) {
      std::this_thread::sleep_for(_delay);
    }
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

  [[nodiscard]] std::size_t scopeEntryCalls() const noexcept {
    return _scopeEntryCalls.load(std::memory_order_relaxed);
  }
  [[nodiscard]] std::size_t visitCalls() const noexcept {
    return _visitCalls.load(std::memory_order_relaxed);
  }

private:
  std::vector<workspace::AstNodeDescription> _entries;
  std::chrono::microseconds _delay;
  mutable std::atomic<std::size_t> _scopeEntryCalls{0};
  mutable std::atomic<std::size_t> _visitCalls{0};
};

struct SingleRefFixture {
  std::shared_ptr<workspace::Document> document;
  ConcurrentReferrer *referrer = nullptr;
  ConcurrentNode *target = nullptr;
};

struct MultiRefFixture {
  std::shared_ptr<workspace::Document> document;
  ConcurrentMultiReferrer *referrer = nullptr;
  ConcurrentNode *target = nullptr;
};

SingleRefFixture
make_single_ref_fixture(pegium::SharedCoreServices &shared,
                        const Linker &linker, std::string uri) {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(std::move(uri), "concref", "a"));
  shared.workspace.documents->addDocument(document);

  auto cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  document->parseResult.astArena = std::make_unique<pegium::AstArena>(*cst);
  auto &arena = *document->parseResult.astArena;
  arena.attachDocument(*document);

  auto *root = arena.create<ConcurrentRoot>();
  static const DummyLit literal;
  CstBuilder builder(*cst);
  builder.leaf(0, 1, &literal);

  auto *target = arena.create<ConcurrentNode>();
  target->name = "a";
  target->setCstNode(cst->get(0));
  root->nodes.push_back(target);
  target->setContainer(*root);

  static const TestRefAssignment<ConcurrentReferrer, ConcurrentNode> assignment(
      "node");
  auto *referrer = arena.create<ConcurrentReferrer>();
  referrer->setCstNode(cst->get(0));
  referrer->node.initialize(*referrer, "a", cst->get(0), assignment, linker);
  root->referrers.push_back(referrer);
  referrer->setContainer(*root);

  document->parseResult.value = root;
  document->parseResult.cst = std::move(cst);
  document->parseResult.references.push_back(
      ReferenceHandle::direct(&referrer->node));
  document->state = workspace::DocumentState::ComputedScopes;

  return {.document = std::move(document),
          .referrer = referrer,
          .target = target};
}

MultiRefFixture
make_multi_ref_fixture(pegium::SharedCoreServices &shared,
                       const Linker &linker, std::string uri) {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(std::move(uri), "concref", "a"));
  shared.workspace.documents->addDocument(document);

  auto cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  document->parseResult.astArena = std::make_unique<pegium::AstArena>(*cst);
  auto &arena = *document->parseResult.astArena;
  arena.attachDocument(*document);

  auto *root = arena.create<ConcurrentRoot>();
  static const DummyLit literal;
  CstBuilder builder(*cst);
  builder.leaf(0, 1, &literal);

  auto *target = arena.create<ConcurrentNode>();
  target->name = "a";
  target->setCstNode(cst->get(0));
  root->nodes.push_back(target);
  target->setContainer(*root);

  static const TestRefAssignment<ConcurrentMultiReferrer, ConcurrentNode, true>
      assignment("nodes");
  auto *referrer = arena.create<ConcurrentMultiReferrer>();
  referrer->setCstNode(cst->get(0));
  referrer->nodes.initialize(*referrer, "a", cst->get(0), assignment, linker);
  root->multiReferrers.push_back(referrer);
  referrer->setContainer(*root);

  document->parseResult.value = root;
  document->parseResult.cst = std::move(cst);
  document->parseResult.references.push_back(
      ReferenceHandle::direct(&referrer->nodes));
  document->state = workspace::DocumentState::ComputedScopes;

  return {.document = std::move(document),
          .referrer = referrer,
          .target = target};
}

constexpr int kThreadCount = 32;

TEST(ConcurrentReferenceResolution, SingleReferenceResolvedExactlyOnce) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "concref");
  pegium::installDefaultCoreServices(*services);

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  auto fixture = make_single_ref_fixture(
      *shared, *linker, test::make_file_uri("concurrent-single.concref"));

  auto scopeProvider = std::make_unique<CountingDelayingScopeProvider>(
      std::vector<workspace::AstNodeDescription>{
          {.name = "a",
           .type = std::type_index(typeid(ConcurrentNode)),
           .documentId = fixture.document->id,
           .symbolId = fixture.document->makeSymbolId(*fixture.target)}},
      std::chrono::milliseconds(5));
  const auto *scopeProviderPtr = scopeProvider.get();
  services->references.scopeProvider = std::move(scopeProvider);
  shared->serviceRegistry->registerServices(std::move(services));

  std::latch start{kThreadCount};
  std::vector<const ConcurrentNode *> results(kThreadCount, nullptr);
  std::vector<std::exception_ptr> exceptions(kThreadCount);
  std::vector<std::jthread> workers;
  workers.reserve(kThreadCount);
  for (int i = 0; i < kThreadCount; ++i) {
    workers.emplace_back([&, i]() {
      start.arrive_and_wait();
      try {
        results[i] = fixture.referrer->node.get();
      } catch (...) {
        exceptions[i] = std::current_exception();
      }
    });
  }
  workers.clear();

  for (int i = 0; i < kThreadCount; ++i) {
    EXPECT_FALSE(exceptions[i])
        << "thread " << i << " threw during ref.get()";
    EXPECT_EQ(results[i], fixture.target)
        << "thread " << i << " got wrong target";
  }
  EXPECT_EQ(scopeProviderPtr->scopeEntryCalls(), 1u)
      << "scope provider should be called exactly once across all threads";
  EXPECT_TRUE(fixture.referrer->node.isResolved());
}

TEST(ConcurrentReferenceResolution, MultiReferenceResolvedExactlyOnce) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "concref");
  pegium::installDefaultCoreServices(*services);

  auto *linker = services->references.linker.get();
  ASSERT_NE(linker, nullptr);

  auto fixture = make_multi_ref_fixture(
      *shared, *linker, test::make_file_uri("concurrent-multi.concref"));

  auto scopeProvider = std::make_unique<CountingDelayingScopeProvider>(
      std::vector<workspace::AstNodeDescription>{
          {.name = "a",
           .type = std::type_index(typeid(ConcurrentNode)),
           .documentId = fixture.document->id,
           .symbolId = fixture.document->makeSymbolId(*fixture.target)}},
      std::chrono::milliseconds(5));
  const auto *scopeProviderPtr = scopeProvider.get();
  services->references.scopeProvider = std::move(scopeProvider);
  shared->serviceRegistry->registerServices(std::move(services));

  std::latch start{kThreadCount};
  std::vector<std::size_t> sizes(kThreadCount, 0);
  std::vector<const ConcurrentNode *> firstItems(kThreadCount, nullptr);
  std::vector<std::exception_ptr> exceptions(kThreadCount);
  std::vector<std::jthread> workers;
  workers.reserve(kThreadCount);
  for (int i = 0; i < kThreadCount; ++i) {
    workers.emplace_back([&, i]() {
      start.arrive_and_wait();
      try {
        const auto items = fixture.referrer->nodes.items();
        sizes[i] = items.size();
        if (!items.empty()) {
          firstItems[i] = items.front().ref;
        }
      } catch (...) {
        exceptions[i] = std::current_exception();
      }
    });
  }
  workers.clear();

  for (int i = 0; i < kThreadCount; ++i) {
    EXPECT_FALSE(exceptions[i])
        << "thread " << i << " threw during MultiReference items()";
    EXPECT_EQ(sizes[i], 1u) << "thread " << i << " saw wrong items size";
    EXPECT_EQ(firstItems[i], fixture.target)
        << "thread " << i << " got wrong first item";
  }
  EXPECT_EQ(scopeProviderPtr->visitCalls(), 1u)
      << "scope provider should visit candidates exactly once";
  EXPECT_TRUE(fixture.referrer->nodes.isResolved());
}

} // namespace
} // namespace pegium::references
