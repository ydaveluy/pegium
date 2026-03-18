#include <pegium/lsp/DefaultFuzzyMatcher.hpp>

#include <optional>

namespace pegium::lsp {

namespace {

constexpr unsigned char a = static_cast<unsigned char>('a');
constexpr unsigned char z = static_cast<unsigned char>('z');
constexpr unsigned char A = static_cast<unsigned char>('A');
constexpr unsigned char Z = static_cast<unsigned char>('Z');
constexpr unsigned char underscore = static_cast<unsigned char>('_');

unsigned char to_upper(unsigned char value) {
  if (a <= value && value <= z) {
    return static_cast<unsigned char>(value - 32);
  }
  return value;
}

bool is_word_transition(unsigned char previous, unsigned char current) {
  return (a <= previous && previous <= z && A <= current && current <= Z) ||
         (previous == underscore && current != underscore);
}

} // namespace

bool DefaultFuzzyMatcher::match(std::string_view query,
                                std::string_view text) const {
  if (query.empty()) {
    return true;
  }

  bool matchedFirstCharacter = false;
  std::optional<unsigned char> previous;
  std::size_t character = 0;
  for (const auto currentChar : text) {
    const auto current =
        static_cast<unsigned char>(currentChar);
    if (const auto expected =
            static_cast<unsigned char>(query[character]);
        current == expected || to_upper(current) == to_upper(expected)) {
      matchedFirstCharacter = matchedFirstCharacter ||
                              !previous.has_value() ||
                              is_word_transition(*previous, current);
      if (matchedFirstCharacter) {
        ++character;
        if (character == query.size()) {
          return true;
        }
      }
    }
    previous = current;
  }
  return false;
}

} // namespace pegium::lsp
