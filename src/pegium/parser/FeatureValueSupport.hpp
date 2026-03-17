#pragma once

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <pegium/grammar/FeatureValue.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/Reference.hpp>

namespace pegium::parser::detail {

struct FeatureValueSupport {
private:
  template <std::ranges::input_range Range>
  [[nodiscard]] static grammar::FeatureValue
  read_feature_value_array(const Range &values) {
    grammar::FeatureValue::Array out;
    if constexpr (requires { std::ranges::size(values); }) {
      out.reserve(static_cast<std::size_t>(std::ranges::size(values)));
    }
    for (const auto &value : values) {
      out.emplace_back(read_feature_value(value));
    }
    return grammar::FeatureValue(std::move(out));
  }

  template <typename T>
  [[nodiscard]] static grammar::FeatureValue make_feature_value(const T &value) {
    using V = std::remove_cvref_t<T>;
    if constexpr (std::derived_from<V, AstNode>) {
      return grammar::FeatureValue(static_cast<const AstNode *>(&value));
    } else if constexpr (std::same_as<V, std::string_view>) {
      return grammar::FeatureValue(grammar::RuleValue(std::string(value)));
    } else if constexpr (detail::SupportedRuleValueType<V>) {
      return grammar::FeatureValue(detail::toRuleValue(value));
    } else {
      return grammar::FeatureValue(grammar::RuleValue(nullptr));
    }
  }

public:
  template <typename T>
  [[nodiscard]] static grammar::FeatureValue
  read_feature_value(const std::optional<T> &value) {
    if (!value.has_value()) {
      return grammar::FeatureValue(grammar::RuleValue(nullptr));
    }
    return read_feature_value(*value);
  }

  template <typename... Ts>
  [[nodiscard]] static grammar::FeatureValue
  read_feature_value(const std::variant<Ts...> &value) {
    return std::visit(
        []<typename T>(const T &item) -> grammar::FeatureValue {
          return read_feature_value(item);
        },
        value);
  }

  template <typename T>
  [[nodiscard]] static grammar::FeatureValue
  read_feature_value(const std::unique_ptr<T> &value) {
    if (!value) {
      return grammar::FeatureValue(grammar::RuleValue(nullptr));
    }
    if constexpr (std::derived_from<T, AstNode>) {
      return grammar::FeatureValue(static_cast<const AstNode *>(value.get()));
    } else {
      return make_feature_value(*value);
    }
  }

  template <typename T>
  [[nodiscard]] static grammar::FeatureValue
  read_feature_value(const T &value)
    requires std::derived_from<T, AbstractReference>
  {
    return grammar::FeatureValue(
        static_cast<const AbstractReference *>(&value));
  }

  template <typename Range>
  [[nodiscard]] static grammar::FeatureValue
  read_feature_value(const Range &values)
    requires std::ranges::input_range<Range> &&
             (!std::same_as<std::remove_cvref_t<Range>, std::string>) &&
             (!std::same_as<std::remove_cvref_t<Range>, std::string_view>)
  {
    return read_feature_value_array(values);
  }

  template <typename T>
  [[nodiscard]] static grammar::FeatureValue read_feature_value(const T &value) {
    return make_feature_value(value);
  }

  template <auto feature, typename ClassType, typename AttrType>
  [[nodiscard]] static grammar::FeatureValue
  get_value(const AstNode *current, AttrType ClassType::*) {
    const auto *obj = static_cast<const ClassType *>(current);
    return read_feature_value(obj->*feature);
  }
};

} // namespace pegium::parser::detail
