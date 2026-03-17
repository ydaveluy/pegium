#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace pegium::utils {

[[nodiscard]] std::string normalize_uri(std::string_view uri);
[[nodiscard]] bool is_file_uri(std::string_view uri);
[[nodiscard]] std::optional<std::string> file_uri_to_path(std::string_view uri);
[[nodiscard]] std::string path_to_file_uri(std::string_view path);
[[nodiscard]] bool equals_uri(std::string_view left, std::string_view right);
[[nodiscard]] bool contains_uri(std::string_view parent, std::string_view child);
[[nodiscard]] std::string relative_uri(std::string_view from, std::string_view to);

} // namespace pegium::utils
