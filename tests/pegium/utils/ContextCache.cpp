#include <gtest/gtest.h>

#include <optional>
#include <string>

#include <pegium/utils/Caching.hpp>

namespace pegium::utils {
namespace {

TEST(ContextCacheTest, SeparatesEntriesByContextAndCanClearOneContext) {
  ContextCache<std::string, std::string, std::string> cache;

  cache.set("doc-1", "key", "first");
  cache.set("doc-2", "key", "second");

  EXPECT_EQ(cache.get("doc-1", "key"), std::optional<std::string>("first"));
  EXPECT_EQ(cache.get("doc-2", "key"), std::optional<std::string>("second"));

  cache.clear("doc-1");
  EXPECT_FALSE(cache.has("doc-1", "key"));
  EXPECT_EQ(cache.get("doc-2", "key"), std::optional<std::string>("second"));
}

} // namespace
} // namespace pegium::utils
