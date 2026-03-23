#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <pegium/core/utils/TransparentStringHash.hpp>

namespace pegium::services {

/// Lightweight owned JSON value used by configuration and protocol helpers.
class JsonValue {
public:
  /// Formatting options used by `toJsonString(...)`.
  struct SerializationOptions {
    bool pretty = true;
    std::size_t indentSize = 2;
  };

  using Array = std::vector<JsonValue>;
  using Object = utils::TransparentStringMap<JsonValue>;
  using Storage =
      std::variant<std::nullptr_t, bool, std::int64_t, double, std::string,
                   Array, Object>;

  JsonValue() noexcept;
  explicit(false) JsonValue(std::nullptr_t) noexcept;
  explicit(false) JsonValue(bool value) noexcept;
  explicit(false) JsonValue(int value) noexcept;
  explicit(false) JsonValue(std::int64_t value) noexcept;
  explicit(false) JsonValue(double value) noexcept;
  explicit(false) JsonValue(std::string value) noexcept;
  explicit(false) JsonValue(const char *value);
  explicit(false) JsonValue(Array value) noexcept;
  explicit(false) JsonValue(Object value) noexcept;

  /// Returns whether the value stores `null`.
  [[nodiscard]] bool isNull() const noexcept;
  /// Returns whether the value stores a boolean.
  [[nodiscard]] bool isBoolean() const noexcept;
  /// Returns whether the value stores an integer.
  [[nodiscard]] bool isInteger() const noexcept;
  /// Returns whether the value stores either an integer or a floating-point number.
  [[nodiscard]] bool isNumber() const noexcept;
  /// Returns whether the value stores a string.
  [[nodiscard]] bool isString() const noexcept;
  /// Returns whether the value stores an array.
  [[nodiscard]] bool isArray() const noexcept;
  /// Returns whether the value stores an object.
  [[nodiscard]] bool isObject() const noexcept;

  [[nodiscard]] bool boolean() const;
  [[nodiscard]] std::int64_t integer() const;
  [[nodiscard]] double number() const;
  [[nodiscard]] const std::string &string() const;
  [[nodiscard]] const Array &array() const;
  [[nodiscard]] Array &array();
  [[nodiscard]] const Object &object() const;
  [[nodiscard]] Object &object();
  [[nodiscard]] std::string toJsonString() const;
  [[nodiscard]] std::string
  toJsonString(const SerializationOptions &options) const;

  friend std::ostream &operator<<(std::ostream &os, const JsonValue &value) {
    os << value.toJsonString();
    return os;
  }

private:
  Storage _value;
};

} // namespace pegium::services
