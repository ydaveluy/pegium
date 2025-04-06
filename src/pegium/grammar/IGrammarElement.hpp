

#pragma once
#include <concepts>
#include <iostream>
#include <pegium/grammar/IContext.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>
#include <type_traits>

namespace pegium::grammar {

/// parse functions returns PARSE_ERROR in case of error
/*static constexpr std::size_t PARSE_ERROR =
    std::numeric_limits<std::size_t>::max();
/// check if the result of parsing succeeded
constexpr bool success(std::size_t len) { return len != PARSE_ERROR; }
/// check if the result of parsing failed
constexpr bool fail(std::size_t len) { return len == PARSE_ERROR; }*/

enum class GrammarElementKind {
  Action,
  AndPredicate,
  AnyCharacter,
  Assignment,
  CharacterRange,
  DataTypeRule,
  Group,
  Literal,
  NotPredicate,
  OrderedChoice,
  ParserRule,
  Repetition,
  TerminalRule,
  UnorderedGroup
};

struct MatchResult {
  const char *offset;
  bool valid;
  constexpr operator bool() const { return valid; }

  constexpr static MatchResult failure(const char *off) { return {off, false}; }

  constexpr static MatchResult success(const char *off) { return {off, true}; }
  /*friend constexpr MatchResult operator+(const MatchResult &r, std::size_t n) {
    return {r.offset + n, r.valid};
  }*/
  /*constexpr MatchResult &operator+=(std::size_t n) {
    offset += n;
    return *this;
  }*/

  constexpr MatchResult &operator|=(const MatchResult &other) {
    if (other.offset > offset) {
      offset = other.offset;
    }
    valid |= other.valid;
    return *this;
  }

  /*constexpr MatchResult &operator&=(const MatchResult &other) {
    if (other.offset > offset) {
      offset = other.offset;
    }
    valid &= other.valid;
    return *this;
  }*/
};

struct IGrammarElement {
  constexpr virtual ~IGrammarElement() noexcept = default;
  // parse the input text from a terminal: no hidden/ignored token between
  // elements
  // return the end position and error position
  constexpr virtual MatchResult
  parse_terminal(std::string_view sv) const noexcept = 0;
  // parse the input text from a rule: hidden/ignored token between elements are
  // skipped
  constexpr virtual MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                           IContext &c) const = 0;

  constexpr virtual void print(std::ostream &os) const = 0;
  constexpr virtual GrammarElementKind getKind() const noexcept = 0;

  friend std::ostream &operator<<(std::ostream &os,
                                  const IGrammarElement &obj) {
    obj.print(os);
    return os;
  }
};

template <typename T>
constexpr bool IsGrammarElement =
    std::derived_from<std::remove_cvref_t<T>, IGrammarElement>;

template <typename T>
  requires IsGrammarElement<T>

using GrammarElementType =
    std::conditional_t<std::is_copy_constructible_v<std::remove_cvref_t<T>>,
                       std::remove_cvref_t<T>, T>;

/*template <typename T>
  requires IsGrammarElement<T>
[[nodiscard]] constexpr auto forwardGrammarElement(
    typename std::remove_reference<T>::type &&element) noexcept {

 // std::forward
  if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<T>> &&
                std::is_lvalue_reference_v<T>) {
    return std::remove_cvref_t<T>{element};
  } else {
    static_assert(
        !std::is_lvalue_reference<T>::value,
        "forwardGrammarElement must not be used to convert an rvalue to an lvalue");
    return static_cast<T &&>(element);
  }
}*/

/// Build an array of char (remove the ending '\0')
/// @tparam N the number of char without the ending '\0'
template <std::size_t N> struct range_array_builder {
  std::array<bool, 256> value{};
  explicit(false) constexpr range_array_builder(char const (&s)[N]) {
    std::size_t i = 0;
    while (i < N - 1) {
      if (i + 2 < N - 1 && s[i + 1] == '-') {
        for (auto c = s[i]; c <= s[i + 2]; ++c) {
          value[static_cast<unsigned char>(c)] = true;
        }
        i += 3;
      } else {
        value[static_cast<unsigned char>(s[i])] = true;
        i += 1;
      }
    }
  }
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
      member = std::move(value);
    } else if constexpr (std::is_constructible_v<T, U>) {
      member = T{value};
    } else {
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
                  std::shared_ptr<U> &&value) const {
    member = std::move(value);
  }
  template <typename U>
    requires std::derived_from<U, T> || std::same_as<U, T>
  void operator()(std::shared_ptr<T> &member, U &&value) const {
    member = std::make_shared<U>(std::move(value));
  }
};

template <typename T> struct AssignmentHelper<std::vector<T>> {
  template <typename U>
  void operator()(std::vector<T> &member, U &&value) const {
    if constexpr (std::is_convertible_v<U, T>) {
      member.emplace_back(std::move(value));
    } else if constexpr (std::is_constructible_v<T, U>) {
      member.emplace_back(T{value});
    } else {
      static_assert(false, "not convertible or constructible");
    }
  }
};
template <typename T> struct AssignmentHelper<std::vector<std::shared_ptr<T>>> {
  template <typename U>
    requires std::derived_from<U, T>
  void operator()(std::vector<std::shared_ptr<T>> &member,
                  std::shared_ptr<U> &&value) const {
    member.emplace_back(std::move(value));
  }
  template <typename U>
    requires std::derived_from<U, T>
  void operator()(std::vector<std::shared_ptr<T>> &member, U &&value) const {
    member.emplace_back(std::make_shared<U>(value));
  }
};
} // namespace helpers

} // namespace pegium::grammar