#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <pegium/references/Scope.hpp>

namespace pegium::references {
namespace {

using workspace::AstNodeDescription;

std::vector<std::string>
collect_names(utils::stream<const AstNodeDescription *> stream) {
  std::vector<std::string> names;
  for (const auto *entry : stream) {
    EXPECT_NE(entry, nullptr);
    if (entry == nullptr) {
      continue;
    }
    names.push_back(entry->name);
  }
  return names;
}

TEST(ScopeTest, MapScopeResolvesWithoutDuplicatingStoredNames) {
  MapScope scope({AstNodeDescription{.name = "alpha", .documentId = 1},
                  AstNodeDescription{.name = "beta", .documentId = 1}});

  const auto *entry = scope.getElement("beta");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->name, "beta");
  EXPECT_EQ(entry->documentId, 1u);
  EXPECT_EQ(collect_names(scope.getAllElements()),
            (std::vector<std::string>{"alpha", "beta"}));
}

TEST(ScopeTest, MultiMapScopeReturnsAllMatchingEntries) {
  MultiMapScope scope({AstNodeDescription{.name = "alpha", .documentId = 1},
                       AstNodeDescription{.name = "alpha", .documentId = 2},
                       AstNodeDescription{.name = "beta", .documentId = 3}});

  auto matches = collect_names(scope.getElements("alpha"));
  EXPECT_EQ(matches, (std::vector<std::string>{"alpha", "alpha"}));

  std::vector<workspace::DocumentId> documentIds;
  for (const auto *entry : scope.getElements("alpha")) {
    ASSERT_NE(entry, nullptr);
    documentIds.push_back(entry->documentId);
  }
  std::ranges::sort(documentIds);
  EXPECT_EQ(documentIds, (std::vector<workspace::DocumentId>{1, 2}));
}

} // namespace
} // namespace pegium::references
