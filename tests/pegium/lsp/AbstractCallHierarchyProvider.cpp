#include <gtest/gtest.h>

#include <pegium/lsp/AbstractCallHierarchyProvider.hpp>

#include "AbstractNavigationProviderTestUtils.hpp"

namespace pegium::lsp {
namespace {

using namespace test_navigation;

class TestCallHierarchyProvider final : public AbstractCallHierarchyProvider {
public:
  using AbstractCallHierarchyProvider::AbstractCallHierarchyProvider;

  mutable std::string incomingName;
  mutable std::string outgoingName;

protected:
  std::vector<::lsp::CallHierarchyIncomingCall>
  getIncomingCalls(const AstNode &node,
                   const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    incomingName = static_cast<const NavigationEntry &>(node).name;

    ::lsp::CallHierarchyIncomingCall call{};
    call.from = makeAuxItem("incoming");
    call.fromRanges.push_back(call.from.selectionRange);
    return {std::move(call)};
  }

  std::vector<::lsp::CallHierarchyOutgoingCall>
  getOutgoingCalls(const AstNode &node,
                   const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    outgoingName = static_cast<const NavigationEntry &>(node).name;

    ::lsp::CallHierarchyOutgoingCall call{};
    call.to = makeAuxItem("outgoing");
    call.fromRanges.push_back(call.to.selectionRange);
    return {std::move(call)};
  }

  void customizeCallHierarchyItem(
      const AstNode &, ::lsp::CallHierarchyItem &item) const override {
    item.detail = "custom call";
  }

private:
  ::lsp::CallHierarchyItem makeAuxItem(std::string name) const {
    ::lsp::CallHierarchyItem item{};
    item.name = std::move(name);
    item.kind = ::lsp::SymbolKind::Method;
    item.uri = ::lsp::DocumentUri(::lsp::Uri::parse("file:///tmp/dummy"));
    return item;
  }
};

TEST(AbstractCallHierarchyProviderTest,
     CreatesDefaultItemAndDelegatesIncomingOutgoing) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<NavigationParser>(*shared, "nav", {".nav"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("call-hierarchy.nav"), "nav",
      "entry Alpha\n"
      "use Alpha");
  ASSERT_NE(document, nullptr);

  const auto *services = lookup_services(*shared, "nav");
  ASSERT_NE(services, nullptr);

  TestCallHierarchyProvider provider(*services);

  ::lsp::CallHierarchyPrepareParams prepareParams{};
  prepareParams.position =
      document->offsetToPosition(use_name_offset(*document) + 1);

  const auto items = provider.prepareCallHierarchy(
      *document, prepareParams, utils::default_cancel_token);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].name, "Alpha");
  EXPECT_EQ(items[0].kind, ::lsp::SymbolKind::Method);
  EXPECT_EQ(items[0].uri.toString(), document->uri);
  EXPECT_EQ(items[0].detail.value_or(""), "custom call");

  ::lsp::CallHierarchyIncomingCallsParams incomingParams{};
  incomingParams.item = items[0];
  const auto incoming =
      provider.incomingCalls(incomingParams, utils::default_cancel_token);
  ASSERT_EQ(incoming.size(), 1u);
  EXPECT_EQ(provider.incomingName, "Alpha");

  ::lsp::CallHierarchyOutgoingCallsParams outgoingParams{};
  outgoingParams.item = items[0];
  const auto outgoing =
      provider.outgoingCalls(outgoingParams, utils::default_cancel_token);
  ASSERT_EQ(outgoing.size(), 1u);
  EXPECT_EQ(provider.outgoingName, "Alpha");
}

} // namespace
} // namespace pegium::lsp
