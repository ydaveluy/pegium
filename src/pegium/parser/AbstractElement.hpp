

#pragma once
#include <algorithm>
#include <concepts>
#include <iostream>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/parser/IContext.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>
#include <type_traits>

namespace pegium::parser {

struct MatchResult {
  const char *offset; // pointer to the end of the match
  bool valid;         // validity status of the match
  [[nodiscard]] constexpr operator bool() const noexcept { return valid; }

  [[nodiscard]] constexpr static MatchResult failure(const char *off) {
    return {off, false};
  }

  [[nodiscard]] constexpr static MatchResult success(const char *off) {
    return {off, true};
  }
};

template <typename T>
concept ParserExpression =
    std::derived_from<std::remove_cvref_t<T>, grammar::AbstractElement> &&
    requires(const std::remove_cvref_t<T> &t, std::string_view sv, CstNode &node, IContext &ctx,
             std::ostream &os) {
      { t.parse_terminal(sv) } noexcept -> std::same_as<MatchResult>;
      { t.parse_rule(sv, node, ctx) } -> std::same_as<MatchResult>;
    };

template <ParserExpression T>
using ParserExpressionHolder =
    std::conditional_t<std::is_copy_constructible_v<std::remove_cvref_t<T>>,
                       std::remove_cvref_t<T>, T>;

constexpr std::array<bool, 256> createCharacterRange(std::string_view s) {

  std::array<bool, 256> value{};
  std::size_t i = 0;

  const std::size_t len = s.size();
  bool negate = false;
  if (len > 0 && s[0] == '^') {
    negate = true;
    ++i;
  }

  while (i < len) {
    auto first = s[i];
    if (i + 2 < len && s[i + 1] == '-') {
      auto last = s[i + 2];
      for (auto c = static_cast<unsigned char>(first);
           c <= static_cast<unsigned char>(last); ++c) {
        value[c] = true;
      }
      i += 3;
    } else {
      value[static_cast<unsigned char>(first)] = true;
      ++i;
    }
  }
  if (negate)
    std::ranges::transform(value, value.begin(), std::logical_not{});
  return value;
}
/// Build an array of char (remove the ending '\0')
/// @tparam N the number of char without the ending '\0'
template <std::size_t N> struct range_array_builder {
  std::array<bool, 256> value{};
  constexpr explicit(false) range_array_builder(const char (&s)[N])
      : value{createCharacterRange({s, N})} {}
};

static constexpr const auto isword_lookup =
    range_array_builder{"a-zA-Z0-9_"}.value;
constexpr bool isword(char c) {
  return isword_lookup[static_cast<unsigned char>(c)];
}

consteval auto make_tolower() {
  std::array<unsigned char, 256> lookup{};
  for (int c = 0; c < 256; ++c) {
    if (c >= 'A' && c <= 'Z') {
      lookup[c] = static_cast<unsigned char>(c) + ('a' - 'A');
    } else {
      lookup[c] = static_cast<unsigned char>(c);
    }
  }
  return lookup;
}
static constexpr auto tolower_array = make_tolower();

/// Fast helper function to convert a char to lower case
/// @param c the char to convert
/// @return the lower case char
constexpr char tolower(char c) {
  return static_cast<char>(tolower_array[static_cast<unsigned char>(c)]);
}

namespace helpers {
template <auto Member> struct MemberTraits;

template <typename Class, typename Type, Type Class::*Member>
struct MemberTraits<Member> {
  using ClassType = Class;

  template <typename T> struct AttributeType {
    using type = T;
  };
  template <typename T> struct AttributeType<std::shared_ptr<T>> {
    using type = T;
  };
  template <typename T> struct AttributeType<std::vector<T>> {
    using type = T;
  };
  template <typename T> struct AttributeType<Reference<T>> {
    using type = std::string;
  };
  template <typename T> struct AttributeType<std::vector<std::shared_ptr<T>>> {
    using type = T;
  };

  using AttrType = AttributeType<Type>::type;
};

template <auto Member>
using ClassType = typename MemberTraits<Member>::ClassType;

template <auto Member> using AttrType = typename MemberTraits<Member>::AttrType;

// Generic
template <typename T> struct AssignmentHelper {
  template <typename U> void operator()(T &member, U &&value) const {

    if constexpr (std::is_convertible_v<U, T>) {
      member = std::forward<U>(value);
    } else if constexpr (std::is_constructible_v<T, U>) {
      member = T{std::forward<U>(value)};
    }
    /*else if constexpr(std::is_invocable_v<
      decltype(feature), helpers::ClassType<feature>,
      typename std::remove_cvref_t<Element>::type> ){

      }*/
    else {
      static_assert(false, "not convertible or constructible");
    }
  }
};
template <typename T> struct AssignmentHelper<Reference<T>> {
  // Use cross reference
  /*void operator()(Reference<T> &member, std::string &&value) const {
    member = std::move(value);
  }*/
};
template <typename T> struct AssignmentHelper<std::shared_ptr<T>> {
  template <typename U>
    requires std::derived_from<U, T> || std::same_as<U, T>
  void operator()(std::shared_ptr<T> &member,
                  const std::shared_ptr<U> &value) const {
    member = value;
  }
  template <typename U>
    requires std::derived_from<U, T> || std::same_as<U, T>
  void operator()(std::shared_ptr<T> &member, U &&value) const {
    member = std::make_shared<U>(std::forward<U>(value));
  }
};

template <typename T> struct AssignmentHelper<std::vector<T>> {
  template <typename U>
  void operator()(std::vector<T> &member, U &&value) const {
    if constexpr (std::is_convertible_v<U, T>) {
      member.emplace_back(std::forward<U>(value));
    } else if constexpr (std::is_constructible_v<T, U>) {
      member.emplace_back(T{std::forward<U>(value)});
    } else {
      static_assert(false, "not convertible or constructible");
    }
  }
};
template <typename T> struct AssignmentHelper<std::vector<std::shared_ptr<T>>> {
  template <typename U>
    requires std::derived_from<U, T>
  void operator()(std::vector<std::shared_ptr<T>> &member,
                  const std::shared_ptr<U> &value) const {
    member.emplace_back(value);
  }
  template <typename U>
    requires std::derived_from<U, T>
  void operator()(std::vector<std::shared_ptr<T>> &member, U &&value) const {
    member.emplace_back(std::make_shared<U>(value));
  }
};
} // namespace helpers
} // namespace pegium::parser