

#pragma once
#include <concepts>
#include <iostream>
#include <pegium/grammar/IContext.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>
#include <type_traits>

namespace pegium::grammar {

/// parse functions returns PARSE_ERROR in case of error
static constexpr std::size_t PARSE_ERROR =
    std::numeric_limits<std::size_t>::max();
/// check if the result of parsing succeeded
constexpr bool success(std::size_t len) { return len != PARSE_ERROR; }
/// check if the result of parsing failed
constexpr bool fail(std::size_t len) { return len == PARSE_ERROR; }

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
  RuleCall,
  TerminalRule,
  UnorderedGroup
};

struct IGrammarElement {
  constexpr virtual ~IGrammarElement() noexcept = default;
  // parse the input text from a terminal: no hidden/ignored token between
  // elements
  constexpr virtual std::size_t
  parse_terminal(std::string_view sv) const noexcept = 0;
  // parse the input text from a rule: hidden/ignored token between elements are
  // skipped
  constexpr virtual std::size_t parse_rule(std::string_view sv, CstNode &parent,
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
concept IsGrammarElement =
    std::derived_from<std::remove_cvref_t<T>, IGrammarElement>;

// GrammarElementType<T> ensures that if T is an lvalue reference, it is
// preserved. Otherwise, it removes const, volatile, and reference qualifiers,
// keeping the base type.
template <typename T>
  requires IsGrammarElement<T>
using GrammarElementType = std::conditional_t<std::is_lvalue_reference_v<T>, T,
                                              std::remove_cvref_t<T>>;

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
  void operator()(T &member, T &&value) const { member = std::move(value); }
};
template <typename T> struct AssignmentHelper<Reference<T>> {
  // Use cross reference
  /*void operator()(Reference<T> &member, std::string &&value) const {
    member = std::move(value);
  }*/
};
template <typename T> struct AssignmentHelper<std::shared_ptr<T>> {
  template <typename U>
    requires std::derived_from<U, T> || std::same_as<U,T>
  void operator()(std::shared_ptr<T> &member,
                  std::shared_ptr<U> &&value) const {
    member = std::move(value);
  }
  template <typename U>
    requires std::derived_from<U, T> || std::same_as<U,T>
  void operator()(std::shared_ptr<T> &member, U &&value) const {
    member = std::make_shared<U>(std::move(value));
  }
};

template <typename T> struct AssignmentHelper<std::vector<T>> {
  void operator()(std::vector<T> &member, T &&value) const {
    member.emplace_back(std::move(value));
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