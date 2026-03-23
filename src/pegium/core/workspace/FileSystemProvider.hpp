#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::workspace {

/// Minimal file-system entry metadata.
struct FileSystemNode {
  bool isFile = false;
  bool isDirectory = false;
  std::string uri;
};

/// Abstract file-system access used during workspace discovery and loading.
class FileSystemProvider {
public:
  virtual ~FileSystemProvider() noexcept = default;

  [[nodiscard]] virtual FileSystemNode stat(std::string_view uri) const = 0;
  [[nodiscard]] virtual bool exists(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::vector<std::uint8_t>
  readBinary(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::string readFile(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::vector<FileSystemNode>
  readDirectory(std::string_view uri) const = 0;
};

/// File-system provider that reports an empty workspace.
class EmptyFileSystemProvider final : public FileSystemProvider {
public:
  [[nodiscard]] FileSystemNode stat(std::string_view uri) const override;
  [[nodiscard]] bool exists(std::string_view uri) const override;
  [[nodiscard]] std::vector<std::uint8_t>
  readBinary(std::string_view uri) const override;
  [[nodiscard]] std::string readFile(std::string_view uri) const override;
  [[nodiscard]] std::vector<FileSystemNode>
  readDirectory(std::string_view uri) const override;
};

/// File-system provider backed by the local machine file system.
class LocalFileSystemProvider final : public FileSystemProvider {
public:
  [[nodiscard]] FileSystemNode stat(std::string_view uri) const override;
  [[nodiscard]] bool exists(std::string_view uri) const override;
  [[nodiscard]] std::vector<std::uint8_t>
  readBinary(std::string_view uri) const override;
  [[nodiscard]] std::string readFile(std::string_view uri) const override;
  [[nodiscard]] std::vector<FileSystemNode>
  readDirectory(std::string_view uri) const override;
};

} // namespace pegium::workspace
