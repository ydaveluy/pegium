#include <gtest/gtest.h>

#include <pegium/lsp/AbstractTypeHierarchyProvider.hpp>

#include "AbstractNavigationProviderTestUtils.hpp"

namespace pegium::lsp {
namespace {

using namespace test_navigation;

class TestTypeHierarchyProvider final : public AbstractTypeHierarchyProvider {
public:
  using AbstractTypeHierarchyProvider::AbstractTypeHierarchyProvider;

  mutable std::string supertypeName;
  mutable std::string subtypeName;

protected:
  std::vector<::lsp::TypeHierarchyItem>
  getSupertypes(const AstNode &node,
                const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    supertypeName = static_cast<const NavigationEntry &>(node).name;
    return {makeAuxItem("super")};
  }

  std::vector<::lsp::TypeHierarchyItem>
  getSubtypes(const AstNode &node,
              const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    subtypeName = static_cast<const NavigationEntry &>(node).name;
    return {makeAuxItem("sub")};
  }

  void customizeTypeHierarchyItem(
      const AstNode &, ::lsp::TypeHierarchyItem &item) const override {
    item.detail = "custom type";
  }

private:
  ::lsp::TypeHierarchyItem makeAuxItem(std::string name) const {
    ::lsp::TypeHierarchyItem item{};
    item.name = std::move(name);
    item.kind = ::lsp::SymbolKind::Class;
    item.uri = ::lsp::DocumentUri(::lsp::Uri::parse("file:///tmp/dummy"));
    return item;
  }
};

TEST(AbstractTypeHierarchyProviderTest,
     CreatesDefaultItemAndDelegatesSuperSubTypes) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<NavigationParser>(*shared, "nav", {".nav"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("type-hierarchy.nav"), "nav",
      "entry Alpha\n"
      "use Alpha");
  ASSERT_NE(document, nullptr);

  const auto *services = lookup_services(*shared, "nav");
  ASSERT_NE(services, nullptr);

  TestTypeHierarchyProvider provider(*services);

  ::lsp::TypeHierarchyPrepareParams prepareParams{};
  prepareParams.position =
      document->offsetToPosition(use_name_offset(*document) + 1);

  const auto items = provider.prepareTypeHierarchy(
      *document, prepareParams, utils::default_cancel_token);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].name, "Alpha");
  EXPECT_EQ(items[0].kind, ::lsp::SymbolKind::Class);
  EXPECT_EQ(items[0].uri.toString(), document->uri);
  EXPECT_EQ(items[0].detail.value_or(""), "custom type");

  ::lsp::TypeHierarchySupertypesParams supertypesParams{};
  supertypesParams.item = items[0];
  const auto supertypes =
      provider.supertypes(supertypesParams, utils::default_cancel_token);
  ASSERT_EQ(supertypes.size(), 1u);
  EXPECT_EQ(provider.supertypeName, "Alpha");

  ::lsp::TypeHierarchySubtypesParams subtypesParams{};
  subtypesParams.item = items[0];
  const auto subtypes =
      provider.subtypes(subtypesParams, utils::default_cancel_token);
  ASSERT_EQ(subtypes.size(), 1u);
  EXPECT_EQ(provider.subtypeName, "Alpha");
}

} // namespace
} // namespace pegium::lsp
