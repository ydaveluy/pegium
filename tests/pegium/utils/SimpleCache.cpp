#include <gtest/gtest.h>

#include <optional>
#include <string>

#include <pegium/utils/Caching.hpp>

namespace pegium::utils {
namespace {

TEST(SimpleCacheTest, SupportsBasicSetGetProviderAndEraseOperations) {
  SimpleCache<std::string, std::string> cache;

  EXPECT_FALSE(cache.has("key"));
  EXPECT_FALSE(cache.get("key").has_value());

  cache.set("key", "value");
  EXPECT_TRUE(cache.has("key"));
  EXPECT_EQ(cache.get("key"), std::optional<std::string>("value"));
  EXPECT_EQ(cache.get("key", [] { return std::string("other"); }), "value");
  EXPECT_TRUE(cache.erase("key"));
  EXPECT_FALSE(cache.erase("key"));
  EXPECT_FALSE(cache.has("key"));
}

} // namespace
} // namespace pegium::utils
