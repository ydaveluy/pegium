#include <pegium/core/workspace/FileSystemProvider.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sstream>

#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium::workspace {

namespace {

std::filesystem::path resolve_path(std::string_view uri) {
  if (const auto filePath = utils::file_uri_to_path(uri); filePath.has_value()) {
    return std::filesystem::path(*filePath);
  }
  return std::filesystem::path(uri);
}

[[nodiscard]] utils::FileSystemError
missing_file_system_node(std::string_view action, std::string_view uri) {
  return utils::FileSystemError(std::format(
      "Failed to {} '{}': file system node is missing or inaccessible.", action,
      uri));
}

FileSystemNode
make_file_system_node(const std::filesystem::directory_entry &entry) {
  return FileSystemNode{
      .isFile = entry.is_regular_file(),
      .isDirectory = entry.is_directory(),
      .uri = utils::path_to_file_uri(entry.path().string())};
}

FileSystemNode make_file_system_node(const std::filesystem::path &path) {
  const auto status = std::filesystem::status(path);
  return FileSystemNode{
      .isFile = std::filesystem::is_regular_file(status),
      .isDirectory = std::filesystem::is_directory(status),
      .uri = utils::path_to_file_uri(path.string())};
}

} // namespace

FileSystemNode EmptyFileSystemProvider::stat(std::string_view uri) const {
  throw utils::FileSystemError(
      std::format("No file system is available for '{}'.", uri));
}

bool EmptyFileSystemProvider::exists(std::string_view uri) const {
  (void)uri;
  return false;
}

std::vector<std::uint8_t>
EmptyFileSystemProvider::readBinary(std::string_view uri) const {
  throw utils::FileSystemError(
      std::format("No file system is available for '{}'.", uri));
}

std::string EmptyFileSystemProvider::readFile(std::string_view uri) const {
  throw utils::FileSystemError(
      std::format("No file system is available for '{}'.", uri));
}

std::vector<FileSystemNode>
EmptyFileSystemProvider::readDirectory(std::string_view uri) const {
  (void)uri;
  return {};
}

FileSystemNode LocalFileSystemProvider::stat(std::string_view uri) const {
  const auto path = resolve_path(uri);
  std::error_code ec;
  const auto status = std::filesystem::status(path, ec);
  if (ec || (!std::filesystem::is_regular_file(status) &&
             !std::filesystem::is_directory(status))) {
    throw missing_file_system_node("stat", uri);
  }
  return make_file_system_node(path);
}

bool LocalFileSystemProvider::exists(std::string_view uri) const {
  return std::filesystem::exists(resolve_path(uri));
}

std::vector<std::uint8_t>
LocalFileSystemProvider::readBinary(std::string_view uri) const {
  std::ifstream stream(resolve_path(uri), std::ios::binary);
  if (!stream.is_open()) {
    throw missing_file_system_node("read binary file", uri);
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), {});
}

std::string LocalFileSystemProvider::readFile(std::string_view uri) const {
  std::ifstream stream(resolve_path(uri), std::ios::binary);
  if (!stream.is_open()) {
    throw missing_file_system_node("read file", uri);
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::vector<FileSystemNode>
LocalFileSystemProvider::readDirectory(std::string_view uri) const {
  std::vector<FileSystemNode> entries;
  const auto directory = resolve_path(uri);
  std::error_code ec;
  auto iterator = std::filesystem::directory_iterator(directory, ec);
  if (ec) {
    throw missing_file_system_node("read directory", uri);
  }
  for (const auto &entry : iterator) {
    entries.push_back(make_file_system_node(entry));
  }
  return entries;
}

} // namespace pegium::workspace
