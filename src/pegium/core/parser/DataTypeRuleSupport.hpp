#pragma once

/// Conversion helpers backing parser data-type rules.

#include <cassert>
#include <functional>
#include <pegium/core/grammar/AnyCharacter.hpp>
#include <pegium/core/grammar/CharacterRange.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/grammar/TerminalRule.hpp>
#include <pegium/core/parser/RuleOptions.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pegium::parser::detail {

template <typename T> struct DataTypeRuleValueSupport {
  static void append_text_fragment(std::string &out,
                                   std::string_view text) {
    out.append(text.data(), text.size());
  }

  template <typename Rule>
  [[nodiscard]] static std::string
  generic_conversion_error_message(const Rule &rule) {
    return "Converter failed for rule " + std::string(rule.getName());
  }

  template <typename Rule>
  [[nodiscard]] static std::string missing_converter_message(const Rule &rule) {
    return "ValueConvert not provided for rule " + std::string(rule.getName());
  }

  [[nodiscard]] static std::string
  unsupported_element_conversion_message(grammar::ElementKind kind) {
    return "ValueConvert not provided for rule " +
           std::to_string(static_cast<int>(kind));
  }

  template <typename Rule>
  static void report_conversion_failure(const Rule &rule, const CstNodeView &node,
                                        const ValueBuildContext *context,
                                        std::string message) {
    if (context == nullptr) {
      return;
    }
    context->addConversionDiagnostic(node, std::addressof(rule),
                                     std::move(message));
  }

  template <typename Rule>
  static T convert_default_value(const Rule &rule, const CstNodeView &node,
                                 const ValueBuildContext *context) {
    if constexpr (std::is_same_v<T, std::string>) {
      std::string value;
      value.reserve(node.getText().size());
      for (const auto it : node) {
        const auto &child = it.node();
        if (child.isHidden) {
          continue;
        }

        const auto *grammarElement = child.grammarElement;
        assert(grammarElement);

        using enum grammar::ElementKind;
        switch (grammarElement->getKind()) {
        case Literal:
        case CharacterRange:
        case AnyCharacter:
          append_text_fragment(value, it.getText());
          break;
        case TerminalRule:
          static_cast<const grammar::TerminalRule *>(grammarElement)
              ->appendTextValue(value, it, context);
          break;
        default:
          report_conversion_failure(
              rule, node, context,
              unsupported_element_conversion_message(grammarElement->getKind()));
          return {};
        }
      }
      return value;
    } else {
      report_conversion_failure(rule, node, context,
                                missing_converter_message(rule));
      return {};
    }
  }

  template <typename Rule, typename Converter>
  static T convert_value(const Rule &rule, const CstNodeView &node,
                         const ValueBuildContext *context,
                         bool hasCustomValueConverter,
                         const Converter &value_converter) {
    if (!hasCustomValueConverter) {
      return convert_default_value(rule, node, context);
    }
    auto result = value_converter(node, context);
    if (result.has_value()) {
      return std::move(result).value();
    }
    report_conversion_failure(
        rule, node, context,
        result.error().empty() ? generic_conversion_error_message(rule)
                               : std::string(result.error()));
    return {};
  }

  template <typename ConverterStorage, typename Converter>
  static void set_value_converter_from_option(ConverterStorage &storage,
                                              bool &hasCustomValueConverter,
                                              Converter &&converter) {
    using ConverterType = std::remove_cvref_t<Converter>;
    if constexpr (std::is_nothrow_invocable_v<ConverterType &,
                                              const CstNodeView &>) {
      using ReturnType =
          std::invoke_result_t<ConverterType &, const CstNodeView &>;
      static_assert(
          opt::IsConversionResultFor_v<ReturnType, T>,
          "DataTypeRule converter must return "
          "opt::ConversionResult<T> (or a compatible value type).");
      storage = [converterFn = std::forward<Converter>(converter)](
                    const CstNodeView &node, const ValueBuildContext *)
                    mutable -> opt::ConversionResult<T> {
        auto result = std::invoke(converterFn, node);
        if (result.has_value()) {
          return opt::conversion_value<T>(
              static_cast<T>(std::move(result).value()));
        }
        return opt::conversion_error<T>(result.error());
      };
      hasCustomValueConverter = true;
    } else if constexpr (std::is_nothrow_invocable_v<ConverterType &,
                                                     std::string_view>) {
      using ReturnType =
          std::invoke_result_t<ConverterType &, std::string_view>;
      static_assert(
          opt::IsConversionResultFor_v<ReturnType, T>,
          "DataTypeRule converter must return "
          "opt::ConversionResult<T> (or a compatible value type).");
      storage = [converterFn = std::forward<Converter>(converter)](
                    const CstNodeView &node, const ValueBuildContext *)
                    mutable -> opt::ConversionResult<T> {
        auto result = std::invoke(converterFn, node.getText());
        if (result.has_value()) {
          return opt::conversion_value<T>(
              static_cast<T>(std::move(result).value()));
        }
        return opt::conversion_error<T>(result.error());
      };
      hasCustomValueConverter = true;
    } else {
      static_assert(
          opt::detail::DependentFalse_v<ConverterType>,
          "DataTypeRule converter must be noexcept and invocable with "
          "const CstNodeView& or std::string_view.");
    }
  }
};

} // namespace pegium::parser::detail
