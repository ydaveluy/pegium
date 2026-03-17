#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::workspace {

struct FileSystemNode {
  bool isFile = false;
  bool isDirectory = false;
  std::string uri;
};

class FileSystemProvider {
public:
  virtual ~FileSystemProvider() noexcept = default;

  [[nodiscard]] virtual FileSystemNode stat(std::string_view uri) const = 0;
  [[nodiscard]] virtual bool exists(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::vector<std::uint8_t>
  readBinary(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  readFile(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::vector<FileSystemNode>
  readDirectory(std::string_view uri) const = 0;
};

class EmptyFileSystemProvider final : public FileSystemProvider {
public:
  [[nodiscard]] FileSystemNode stat(std::string_view uri) const override;
  [[nodiscard]] bool exists(std::string_view uri) const override;
  [[nodiscard]] std::vector<std::uint8_t>
  readBinary(std::string_view uri) const override;
  [[nodiscard]] std::optional<std::string>
  readFile(std::string_view uri) const override;
  [[nodiscard]] std::vector<FileSystemNode>
  readDirectory(std::string_view uri) const override;
};

class LocalFileSystemProvider final : public FileSystemProvider {
public:
  [[nodiscard]] FileSystemNode stat(std::string_view uri) const override;
  [[nodiscard]] bool exists(std::string_view uri) const override;
  [[nodiscard]] std::vector<std::uint8_t>
  readBinary(std::string_view uri) const override;
  [[nodiscard]] std::optional<std::string>
  readFile(std::string_view uri) const override;
  [[nodiscard]] std::vector<FileSystemNode>
  readDirectory(std::string_view uri) const override;
};

} // namespace pegium::workspace
