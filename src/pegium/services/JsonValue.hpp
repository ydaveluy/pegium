#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace pegium::services {

class JsonValue {
public:
  struct SerializationOptions {
    bool pretty = true;
    std::size_t indentSize = 2;
  };

  using Array = std::vector<JsonValue>;
  using Object = std::unordered_map<std::string, JsonValue>;
  using Storage =
      std::variant<std::nullptr_t, bool, std::int64_t, double, std::string,
                   Array, Object>;

  JsonValue() noexcept;
  JsonValue(std::nullptr_t) noexcept;
  JsonValue(bool value) noexcept;
  JsonValue(int value) noexcept;
  JsonValue(std::int64_t value) noexcept;
  JsonValue(double value) noexcept;
  JsonValue(std::string value) noexcept;
  JsonValue(const char *value);
  JsonValue(Array value) noexcept;
  JsonValue(Object value) noexcept;

  [[nodiscard]] bool isNull() const noexcept;
  [[nodiscard]] bool isBoolean() const noexcept;
  [[nodiscard]] bool isInteger() const noexcept;
  [[nodiscard]] bool isNumber() const noexcept;
  [[nodiscard]] bool isString() const noexcept;
  [[nodiscard]] bool isArray() const noexcept;
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

private:
  Storage _value;
};

std::ostream &operator<<(std::ostream &os, const JsonValue &value);

} // namespace pegium::services
