#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <pegium/utils/UriUtils.hpp>
#include <pegium/workspace/FileSystemProvider.hpp>

namespace pegium::workspace {
namespace {

TEST(EmptyFileSystemProviderTest, ReturnsEmptyOrThrows) {
  EmptyFileSystemProvider provider;

  EXPECT_FALSE(provider.exists("file:///tmp/missing.test"));
  EXPECT_FALSE(provider.readFile("file:///tmp/missing.test").has_value());
  EXPECT_TRUE(provider.readDirectory("file:///tmp/missing").empty());
  EXPECT_THROW((void)provider.stat("file:///tmp/missing.test"),
               std::runtime_error);
  EXPECT_THROW((void)provider.readBinary("file:///tmp/missing.test"),
               std::runtime_error);
}

TEST(LocalFileSystemProviderTest, UsesUrisForStatAndDirectoryEntries) {
  const auto rootPath =
      std::filesystem::path("/tmp/pegium-tests/fs-provider");
  const auto childPath = rootPath / "sample.test";
  std::filesystem::create_directories(rootPath);
  {
    std::ofstream stream(childPath);
    ASSERT_TRUE(stream.is_open());
    stream << "alpha";
  }

  LocalFileSystemProvider provider;
  const auto rootUri = utils::path_to_file_uri(rootPath.string());
  const auto childUri = utils::path_to_file_uri(childPath.string());

  ASSERT_TRUE(provider.exists(rootUri));
  const auto rootStat = provider.stat(rootUri);
  EXPECT_TRUE(rootStat.isDirectory);
  EXPECT_FALSE(rootStat.isFile);
  EXPECT_EQ(rootStat.uri, rootUri);

  const auto childStat = provider.stat(childUri);
  EXPECT_TRUE(childStat.isFile);
  EXPECT_FALSE(childStat.isDirectory);
  EXPECT_EQ(childStat.uri, childUri);

  const auto content = provider.readFile(childUri);
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "alpha");

  const auto entries = provider.readDirectory(rootUri);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front().uri, childUri);
  EXPECT_TRUE(entries.front().isFile);

  std::filesystem::remove_all(rootPath);
}

} // namespace
} // namespace pegium::workspace
