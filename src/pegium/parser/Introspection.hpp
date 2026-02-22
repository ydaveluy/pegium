#pragma once

#include <source_location>
#include <string_view>

namespace pegium::parser::detail {

template <auto... Values>
[[nodiscard]] constexpr auto function_name() noexcept -> std::string_view {
  return std::source_location::current().function_name();
}

template <class... Types>
[[nodiscard]] constexpr auto function_name() noexcept -> std::string_view {
  return std::source_location::current().function_name();
}

template <auto MemberPointer>
struct MemberNameExtractor {
  struct REF_STRUCT {
    int MEMBER;

    static constexpr auto name = function_name<&REF_STRUCT::MEMBER>();
    static constexpr auto end_marker =
        name.substr(name.find("REF_STRUCT::MEMBER") +
                    std::string_view{"REF_STRUCT::MEMBER"}.size());
  };

  static consteval std::string_view value() noexcept {
    std::string_view func_name = function_name<MemberPointer>();
    const auto marker_pos = func_name.rfind(REF_STRUCT::end_marker);
    if (marker_pos != std::string_view::npos) {
      func_name = func_name.substr(0, marker_pos);
    }

    const auto sep = func_name.rfind("::");
    if (sep == std::string_view::npos) {
      return func_name;
    }

    return func_name.substr(sep + 2);
  }
};

template <auto MemberPointer>
consteval std::string_view member_name() noexcept {
  return MemberNameExtractor<MemberPointer>::value();
}

template <auto MemberPointer>
inline constexpr std::string_view member_name_v = member_name<MemberPointer>();

consteval std::string_view simplifyTypeName(std::string_view typeName) noexcept {
  constexpr std::string_view classPrefix = "class ";
  if (typeName.rfind(classPrefix, 0) == 0) {
    typeName.remove_prefix(classPrefix.size());
  }

  constexpr std::string_view structPrefix = "struct ";
  if (typeName.rfind(structPrefix, 0) == 0) {
    typeName.remove_prefix(structPrefix.size());
  }

  if (const auto nsSeparator = typeName.rfind("::");
      nsSeparator != std::string_view::npos) {
    typeName.remove_prefix(nsSeparator + 2);
  }

  return typeName;
}

template <typename Type>
consteval std::string_view computeTypeName() noexcept {
#if defined(__clang__) || defined(__GNUC__)
  constexpr std::string_view functionName = __PRETTY_FUNCTION__;
  constexpr std::string_view marker = "Type = ";
  auto begin = functionName.find(marker);
  if (begin == std::string_view::npos) {
    return simplifyTypeName(functionName);
  }

  begin += marker.size();
  auto end = functionName.find(';', begin);
  if (end == std::string_view::npos) {
    end = functionName.find(']', begin);
  }
  if (end == std::string_view::npos) {
    end = functionName.size();
  }

  return simplifyTypeName(functionName.substr(begin, end - begin));
#elif defined(_MSC_VER)
  constexpr std::string_view functionName = __FUNCSIG__;
  constexpr std::string_view marker = "computeTypeName<";
  auto begin = functionName.find(marker);
  if (begin == std::string_view::npos) {
    return simplifyTypeName(functionName);
  }

  begin += marker.size();
  auto end = functionName.find(">(void)", begin);
  if (end == std::string_view::npos) {
    end = functionName.find('>', begin);
  }
  if (end == std::string_view::npos) {
    end = functionName.size();
  }

  return simplifyTypeName(functionName.substr(begin, end - begin));
#else
  return "Unknown";
#endif
}

template <typename Type>
inline constexpr std::string_view type_name_v = computeTypeName<Type>();

} // namespace pegium::parser::detail
