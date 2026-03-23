#include <pegium/core/services/JsonValue.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <ostream>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

namespace pegium::services {
namespace {

void append_indent(std::string &out, std::size_t count) { out.append(count, ' '); }

void append_escaped_json_string(std::string &out, std::string_view text) {
  static constexpr char hex[] = "0123456789ABCDEF";
  out.push_back('"');
  for (const auto byte :
       std::as_bytes(std::span(text.data(), text.size()))) {
    const auto c = std::to_integer<unsigned char>(byte);
    switch (c) {
    case '\"':
      out += R"(\")";
      break;
    case '\\':
      out += R"(\\)";
      break;
    case '\b':
      out += R"(\b)";
      break;
    case '\f':
      out += R"(\f)";
      break;
    case '\n':
      out += R"(\n)";
      break;
    case '\r':
      out += R"(\r)";
      break;
    case '\t':
      out += R"(\t)";
      break;
    default:
      if (c < 0x20) {
        out += "\\u00";
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0x0F]);
      } else {
        out.push_back(static_cast<char>(c));
      }
      break;
    }
  }
  out.push_back('"');
}

std::string serialize_number(double value) {
  std::ostringstream stream;
  stream.precision(std::numeric_limits<double>::max_digits10);
  stream << value;
  return stream.str();
}

void append_json_value(std::string &out, const JsonValue &value,
                       const JsonValue::SerializationOptions &options,
                       std::size_t indent);

void append_json_array(std::string &out, const JsonValue::Array &array,
                       const JsonValue::SerializationOptions &options,
                       std::size_t indent) {
  if (array.empty()) {
    out += "[]";
    return;
  }

  if (!options.pretty) {
    out.push_back('[');
    for (std::size_t i = 0; i < array.size(); ++i) {
      if (i > 0) {
        out.push_back(',');
      }
      append_json_value(out, array[i], options, indent);
    }
    out.push_back(']');
    return;
  }

  out += "[\n";
  const auto childIndent = indent + options.indentSize;
  for (std::size_t i = 0; i < array.size(); ++i) {
    if (i > 0) {
      out += ",\n";
    }
    append_indent(out, childIndent);
    append_json_value(out, array[i], options, childIndent);
  }
  out += '\n';
  append_indent(out, indent);
  out.push_back(']');
}

void append_json_object(std::string &out, const JsonValue::Object &object,
                        const JsonValue::SerializationOptions &options,
                        std::size_t indent) {
  if (object.empty()) {
    out += "{}";
    return;
  }

  std::vector<std::string_view> keys;
  keys.reserve(object.size());
  for (const auto &[key, _] : object) {
    keys.emplace_back(key);
  }
  std::ranges::sort(keys);

  if (!options.pretty) {
    out.push_back('{');
    for (std::size_t i = 0; i < keys.size(); ++i) {
      if (i > 0) {
        out.push_back(',');
      }
      append_escaped_json_string(out, keys[i]);
      out.push_back(':');
      append_json_value(out, object.at(std::string(keys[i])), options, indent);
    }
    out.push_back('}');
    return;
  }

  out += "{\n";
  const auto childIndent = indent + options.indentSize;
  for (std::size_t i = 0; i < keys.size(); ++i) {
    if (i > 0) {
      out += ",\n";
    }
    append_indent(out, childIndent);
    append_escaped_json_string(out, keys[i]);
    out += ": ";
    append_json_value(out, object.at(std::string(keys[i])), options, childIndent);
  }
  out += '\n';
  append_indent(out, indent);
  out.push_back('}');
}

void append_json_value(std::string &out, const JsonValue &value,
                       const JsonValue::SerializationOptions &options,
                       std::size_t indent) {
  if (value.isNull()) {
    out += "null";
    return;
  }
  if (value.isBoolean()) {
    out += value.boolean() ? "true" : "false";
    return;
  }
  if (value.isInteger()) {
    out += std::to_string(value.integer());
    return;
  }
  if (value.isString()) {
    append_escaped_json_string(out, value.string());
    return;
  }
  if (value.isArray()) {
    append_json_array(out, value.array(), options, indent);
    return;
  }
  if (value.isObject()) {
    append_json_object(out, value.object(), options, indent);
    return;
  }

  out += serialize_number(value.number());
}

} // namespace

JsonValue::JsonValue() noexcept : _value(nullptr) {}
JsonValue::JsonValue(std::nullptr_t) noexcept : _value(nullptr) {}
JsonValue::JsonValue(bool value) noexcept : _value(value) {}
JsonValue::JsonValue(int value) noexcept
    : _value(static_cast<std::int64_t>(value)) {}
JsonValue::JsonValue(std::int64_t value) noexcept : _value(value) {}
JsonValue::JsonValue(double value) noexcept : _value(value) {}
JsonValue::JsonValue(std::string value) noexcept : _value(std::move(value)) {}
JsonValue::JsonValue(const char *value)
    : _value(value == nullptr ? Storage(nullptr) : Storage(std::string(value))) {}
JsonValue::JsonValue(Array value) noexcept : _value(std::move(value)) {}
JsonValue::JsonValue(Object value) noexcept : _value(std::move(value)) {}

bool JsonValue::isNull() const noexcept {
  return std::holds_alternative<std::nullptr_t>(_value);
}
bool JsonValue::isBoolean() const noexcept {
  return std::holds_alternative<bool>(_value);
}
bool JsonValue::isInteger() const noexcept {
  return std::holds_alternative<std::int64_t>(_value);
}
bool JsonValue::isNumber() const noexcept {
  return isInteger() || std::holds_alternative<double>(_value);
}
bool JsonValue::isString() const noexcept {
  return std::holds_alternative<std::string>(_value);
}
bool JsonValue::isArray() const noexcept {
  return std::holds_alternative<Array>(_value);
}
bool JsonValue::isObject() const noexcept {
  return std::holds_alternative<Object>(_value);
}

bool JsonValue::boolean() const {
  return std::get<bool>(_value);
}

std::int64_t JsonValue::integer() const {
  if (const auto *value = std::get_if<std::int64_t>(&_value)) {
    return *value;
  }
  if (const auto *value = std::get_if<double>(&_value)) {
    return static_cast<std::int64_t>(*value);
  }
  throw std::bad_variant_access();
}

double JsonValue::number() const {
  if (const auto *value = std::get_if<double>(&_value)) {
    return *value;
  }
  if (const auto *value = std::get_if<std::int64_t>(&_value)) {
    return static_cast<double>(*value);
  }
  throw std::bad_variant_access();
}

const std::string &JsonValue::string() const {
  return std::get<std::string>(_value);
}

const JsonValue::Array &JsonValue::array() const {
  return std::get<Array>(_value);
}

JsonValue::Array &JsonValue::array() {
  return std::get<Array>(_value);
}

const JsonValue::Object &JsonValue::object() const {
  return std::get<Object>(_value);
}

JsonValue::Object &JsonValue::object() {
  return std::get<Object>(_value);
}

std::string JsonValue::toJsonString() const {
  return toJsonString(SerializationOptions{});
}

std::string JsonValue::toJsonString(const SerializationOptions &options) const {
  std::string out;
  append_json_value(out, *this, options, 0);
  return out;
}

} // namespace pegium::services
