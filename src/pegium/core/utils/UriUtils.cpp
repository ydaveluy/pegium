#include <pegium/core/utils/UriUtils.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace pegium::utils {

namespace {

bool has_file_scheme(std::string_view uri) {
  if (uri.size() < 7) {
    return false;
  }
  for (std::size_t index = 0; index < 7; ++index) {
    const auto actual = static_cast<unsigned char>(uri[index]);
    const auto expected = static_cast<unsigned char>("file://"[index]);
    if (std::tolower(actual) != expected) {
      return false;
    }
  }
  return true;
}

bool is_unreserved(unsigned char c) {
  return std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~' ||
         c == '/' || c == ':';
}

char to_hex(unsigned char value) {
  return static_cast<char>(value < 10 ? ('0' + value) : ('A' + (value - 10)));
}

std::string percent_encode(std::string_view text) {
  std::string encoded;
  encoded.reserve(text.size());

  for (unsigned char c : text) {
    if (is_unreserved(c)) {
      encoded.push_back(static_cast<char>(c));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(to_hex(static_cast<unsigned char>((c >> 4U) & 0x0FU)));
    encoded.push_back(to_hex(static_cast<unsigned char>(c & 0x0FU)));
  }

  return encoded;
}

int from_hex(char c) {
  const auto lower =
      static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower >= '0' && lower <= '9') {
    return lower - '0';
  }
  if (lower >= 'a' && lower <= 'f') {
    return 10 + (lower - 'a');
  }
  return -1;
}

std::string percent_decode(std::string_view text) {
  std::string decoded;
  decoded.reserve(text.size());

  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '%' && index + 2 < text.size()) {
      const auto high = from_hex(text[index + 1]);
      const auto low = from_hex(text[index + 2]);
      if (high >= 0 && low >= 0) {
        decoded.push_back(
            static_cast<char>((static_cast<unsigned int>(high) << 4U) |
                              static_cast<unsigned int>(low)));
        index += 2;
        continue;
      }
    }
    decoded.push_back(text[index]);
  }

  return decoded;
}

std::filesystem::path normalize_file_path(std::string_view path) {
  auto normalized = std::filesystem::path(path).lexically_normal().generic_string();
  while (normalized.size() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  return std::filesystem::path(normalized);
}

} // namespace

std::string normalize_uri(std::string_view uri) {
  if (const auto path = file_uri_to_path(uri); path.has_value()) {
    return path_to_file_uri(*path);
  }
  return std::string(uri);
}

bool is_file_uri(std::string_view uri) {
  return has_file_scheme(uri);
}

std::optional<std::string> file_uri_to_path(std::string_view uri) {
  if (!has_file_scheme(uri)) {
    return std::nullopt;
  }

  std::string_view remainder = uri.substr(7);
  std::string_view path = remainder;

  if (!remainder.empty() && remainder.front() != '/') {
    const auto slashIndex = remainder.find('/');
    const auto authority = slashIndex == std::string_view::npos
                               ? remainder
                               : remainder.substr(0, slashIndex);
    if (!authority.empty()) {
      std::string lowered(authority);
      std::ranges::transform(lowered, lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (lowered != "localhost") {
        return std::nullopt;
      }
    }
    path = slashIndex == std::string_view::npos ? std::string_view{}
                                                : remainder.substr(slashIndex);
  }

  auto decoded = percent_decode(path);
#ifdef _WIN32
  if (decoded.size() >= 3 && decoded[0] == '/' &&
      std::isalpha(static_cast<unsigned char>(decoded[1])) != 0 &&
      decoded[2] == ':') {
    decoded.erase(decoded.begin());
  }
  std::ranges::replace(decoded, '/', '\\');
#endif
  return decoded;
}

std::string path_to_file_uri(std::string_view path) {
  auto normalized = std::filesystem::absolute(std::filesystem::path(path))
                        .lexically_normal()
                        .generic_string();
#ifdef _WIN32
  if (normalized.size() >= 2 &&
      std::isalpha(static_cast<unsigned char>(normalized[0])) != 0 &&
      normalized[1] == ':') {
    normalized.insert(normalized.begin(), '/');
  }
#endif
  if (normalized.empty() || normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return "file://" + percent_encode(normalized);
}

bool equals_uri(std::string_view left, std::string_view right) {
  return normalize_uri(left) == normalize_uri(right);
}

bool contains_uri(std::string_view parent, std::string_view child) {
  const auto normalizedParent = normalize_uri(parent);
  const auto normalizedChild = normalize_uri(child);
  if (normalizedParent == normalizedChild) {
    return true;
  }

  const auto parentPath = file_uri_to_path(normalizedParent);
  const auto childPath = file_uri_to_path(normalizedChild);
  if (!parentPath.has_value() || !childPath.has_value()) {
    return false;
  }

  const auto parentFsPath = normalize_file_path(*parentPath);
  const auto childFsPath = normalize_file_path(*childPath);

  auto parentIt = parentFsPath.begin();
  auto childIt = childFsPath.begin();
  for (; parentIt != parentFsPath.end() && childIt != childFsPath.end();
       ++parentIt, ++childIt) {
    if (*parentIt != *childIt) {
      return false;
    }
  }

  return parentIt == parentFsPath.end();
}

std::string relative_uri(std::string_view from, std::string_view to) {
  const auto normalizedFrom = normalize_uri(from);
  const auto normalizedTo = normalize_uri(to);

  const auto fromPath = file_uri_to_path(normalizedFrom);
  const auto toPath = file_uri_to_path(normalizedTo);
  if (!fromPath.has_value() || !toPath.has_value()) {
    return normalizedTo;
  }

  const auto relative =
      normalize_file_path(*toPath).lexically_relative(normalize_file_path(*fromPath))
          .generic_string();
  if (!relative.empty()) {
    return relative;
  }
  return normalize_file_path(*toPath).generic_string();
}

} // namespace pegium::utils
