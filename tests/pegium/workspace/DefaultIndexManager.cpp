#include <gtest/gtest.h>

#include <string>
#include <typeindex>
#include <vector>

#include <pegium/workspace/DefaultIndexManager.hpp>

namespace pegium::workspace {
namespace {

struct BucketType {};
struct TypeFilterNode {};

std::type_index type_filter() {
  return std::type_index(typeid(TypeFilterNode));
}

std::vector<std::string> collect_names(utils::stream<AstNodeDescription> entries) {
  std::vector<std::string> names;
  for (const auto &entry : entries) {
    names.push_back(entry.name);
  }
  return names;
}

TEST(DefaultIndexManagerTest, InvalidatesTypedExportCacheWhenDocumentChanges) {
  DefaultIndexManager indexManager;
  constexpr DocumentId documentId = 1;

  indexManager.setExports(
      documentId, {{.name = "first",
             .type = std::type_index(typeid(TypeFilterNode)),
             .documentId = documentId}});
  EXPECT_EQ(collect_names(indexManager.allElements(type_filter())),
            (std::vector<std::string>{"first"}));

  indexManager.setExports(
      documentId,
      {{.name = "second",
        .type = std::type_index(typeid(TypeFilterNode)),
        .documentId = documentId}});
  EXPECT_EQ(collect_names(indexManager.allElements(type_filter())),
            (std::vector<std::string>{"second"}));
}

TEST(DefaultIndexManagerTest, InvalidatesTypedExportCacheWhenDocumentIsRemoved) {
  DefaultIndexManager indexManager;
  constexpr DocumentId documentId = 2;

  indexManager.setExports(
      documentId, {{.name = "value",
             .type = std::type_index(typeid(TypeFilterNode)),
             .documentId = documentId}});
  EXPECT_EQ(collect_names(indexManager.allElements(type_filter())),
            (std::vector<std::string>{"value"}));

  EXPECT_TRUE(indexManager.remove(documentId));
  EXPECT_TRUE(collect_names(indexManager.allElements(type_filter())).empty());
}

TEST(DefaultIndexManagerTest,
     InvalidatesBucketedScopeEntriesWhenDocumentChanges) {
  DefaultIndexManager indexManager;
  constexpr DocumentId documentId = 3;

  indexManager.setExports(
      documentId, {{.name = "first",
             .type = std::type_index(typeid(BucketType)),
             .documentId = documentId}});
  const auto initialBuckets = indexManager.allBucketedScopeEntries();
  ASSERT_NE(initialBuckets, nullptr);
  ASSERT_EQ(initialBuckets->buckets.size(), 1u);
  ASSERT_EQ(initialBuckets->buckets.front().entries.size(), 1u);
  EXPECT_EQ(initialBuckets->buckets.front().entries.front()->name, "first");

  indexManager.setExports(
      documentId, {{.name = "second",
             .type = std::type_index(typeid(BucketType)),
             .documentId = documentId}});
  const auto updatedBuckets = indexManager.allBucketedScopeEntries();
  ASSERT_NE(updatedBuckets, nullptr);
  ASSERT_NE(updatedBuckets, initialBuckets);
  ASSERT_EQ(updatedBuckets->buckets.size(), 1u);
  ASSERT_EQ(updatedBuckets->buckets.front().entries.size(), 1u);
  EXPECT_EQ(updatedBuckets->buckets.front().entries.front()->name, "second");
}

} // namespace
} // namespace pegium::workspace
