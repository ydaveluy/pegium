#include <pegium/workspace/FileSystemProvider.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sstream>

#include <pegium/utils/UriUtils.hpp>

namespace pegium::workspace {

namespace {

std::filesystem::path resolve_path(std::string_view uri) {
  if (const auto filePath = utils::file_uri_to_path(uri); filePath.has_value()) {
    return std::filesystem::path(*filePath);
  }
  return std::filesystem::path(uri);
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
  throw std::runtime_error(
      std::format("No file system is available for '{}'.", uri));
}

bool EmptyFileSystemProvider::exists(std::string_view uri) const {
  (void)uri;
  return false;
}

std::vector<std::uint8_t>
EmptyFileSystemProvider::readBinary(std::string_view uri) const {
  throw std::runtime_error(
      std::format("No file system is available for '{}'.", uri));
}

std::optional<std::string>
EmptyFileSystemProvider::readFile(std::string_view uri) const {
  (void)uri;
  return std::nullopt;
}

std::vector<FileSystemNode>
EmptyFileSystemProvider::readDirectory(std::string_view uri) const {
  (void)uri;
  return {};
}

FileSystemNode LocalFileSystemProvider::stat(std::string_view uri) const {
  return make_file_system_node(resolve_path(uri));
}

bool LocalFileSystemProvider::exists(std::string_view uri) const {
  return std::filesystem::exists(resolve_path(uri));
}

std::vector<std::uint8_t>
LocalFileSystemProvider::readBinary(std::string_view uri) const {
  std::ifstream stream(resolve_path(uri), std::ios::binary);
  if (!stream.is_open()) {
    return {};
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), {});
}

std::optional<std::string>
LocalFileSystemProvider::readFile(std::string_view uri) const {
  std::ifstream stream(resolve_path(uri), std::ios::binary);
  if (!stream.is_open()) {
    return std::nullopt;
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
  for (const auto &entry : std::filesystem::directory_iterator(directory, ec)) {
    if (ec) {
      break;
    }
    entries.push_back(make_file_system_node(entry));
  }
  return entries;
}

} // namespace pegium::workspace
