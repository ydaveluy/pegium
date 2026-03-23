#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace pegium::utils {

/// Normalizes a URI for stable comparisons and map keys.
[[nodiscard]] std::string normalize_uri(std::string_view uri);
/// Returns whether `uri` uses the `file:` scheme.
[[nodiscard]] bool is_file_uri(std::string_view uri);
/// Converts a `file:` URI to a local path when possible.
[[nodiscard]] std::optional<std::string> file_uri_to_path(std::string_view uri);
/// Converts a local path to a normalized `file:` URI.
[[nodiscard]] std::string path_to_file_uri(std::string_view path);
/// Returns whether two URIs normalize to the same value.
[[nodiscard]] bool equals_uri(std::string_view left, std::string_view right);
/// Returns whether `child` is located under `parent` in URI space.
[[nodiscard]] bool contains_uri(std::string_view parent, std::string_view child);
/// Returns the relative URI from `from` to `to` when both share a parent hierarchy.
[[nodiscard]] std::string relative_uri(std::string_view from, std::string_view to);

} // namespace pegium::utils
