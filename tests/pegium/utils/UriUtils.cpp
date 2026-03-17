#include <gtest/gtest.h>

#include <string_view>

#include <pegium/utils/UriUtils.hpp>

namespace pegium::utils {
namespace {

TEST(UriUtilsTest, RoundTripsFileUrisAndRejectsNonFileUris) {
  constexpr std::string_view path = "/tmp/pegium-utils-uri.txt";
  const auto uri = path_to_file_uri(path);
  EXPECT_TRUE(is_file_uri(uri));

  const auto decoded = file_uri_to_path(uri);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, path);

  EXPECT_EQ(normalize_uri(uri), uri);
  EXPECT_FALSE(file_uri_to_path("not-a-uri").has_value());
}

TEST(UriUtilsTest, ComputesRelativeUrisForSiblingParentAndChildPaths) {
  EXPECT_EQ(relative_uri("file:///a/b", "file:///a/b/d.txt"), "d.txt");
  EXPECT_EQ(relative_uri("file:///a/b", "file:///a/d.txt"), "../d.txt");
  EXPECT_EQ(relative_uri("file:///a", "file:///a/b/c.txt"), "b/c.txt");
  EXPECT_EQ(relative_uri("file:///a/b", "file:///a/c/d.txt"), "../c/d.txt");
}

TEST(UriUtilsTest, ComparesNormalizedUrisForEquality) {
  EXPECT_TRUE(equals_uri("file:///a/b", "file:///a/b"));
  EXPECT_FALSE(equals_uri("file:///a/b", "file:///c/b"));
  EXPECT_FALSE(equals_uri("file:///a/b", "file:///a/b/c"));
}

TEST(UriUtilsTest, DetectsUriContainmentWithEqualAndChildUris) {
  EXPECT_TRUE(contains_uri("file:///path/to/file", "file:///path/to/file"));
  EXPECT_TRUE(contains_uri("file:///path/to/file/", "file:///path/to/file"));
  EXPECT_TRUE(contains_uri("file:///path/to", "file:///path/to/file"));
  EXPECT_TRUE(contains_uri("file:///path/to/", "file:///path/to/file"));

  EXPECT_FALSE(contains_uri("file:///path/to/file", "file:///path/to"));
  EXPECT_FALSE(
      contains_uri("file:///path/to/directory", "file:///path/to/other"));
}

} // namespace
} // namespace pegium::utils
