#include <cassert>
#include <limits>
#include <pegium/grammar.hpp>
namespace pegium {

consteval std::array<bool, 256> make_lookup() { return {}; }

template <typename... Args>
consteval std::array<bool, 256> make_lookup(char single_char, Args... rest) {
  std::array<bool, 256> result = make_lookup(rest...);
  result[static_cast<unsigned char>(single_char)] = true;
  return result;
}
template <typename... Args>
consteval std::array<bool, 256> make_lookup(std::pair<char, char> range,
                                            Args... rest) {
  std::array<bool, 256> result = make_lookup(rest...);
  for (char c = range.first; c <= range.second; ++c) {
    result[static_cast<unsigned char>(c)] = true;
  }
  return result;
}

inline bool isword(char c) {
  static constexpr auto lookup = make_lookup(
      std::pair{'a', 'z'}, std::pair{'A', 'Z'}, std::pair{'0', '9'}, '_');
  return lookup[static_cast<unsigned char>(c)];
}

consteval std::array<unsigned char, 256> make_tolower() {
  std::array<unsigned char, 256> table{};
  for (int c = 0; c < 256; ++c) {
    if (c >= 'A' && c <= 'Z') {
      table[c] = static_cast<unsigned char>(c) + ('a' - 'A');
    } else {
      table[c] = static_cast<unsigned char>(c);
    }
  }
  return table;
}

inline char tolower(char c) {
  static constexpr auto tolower = make_tolower();
  return tolower[static_cast<unsigned char>(c)];
}

static constexpr std::size_t PARSE_ERROR =
    std::numeric_limits<std::size_t>::max();
inline bool success(size_t len) { return len != PARSE_ERROR; }

inline bool fail(size_t len) { return len == PARSE_ERROR; }
/*-----------------------------------------------------------------------------
 *  UTF8 functions
 *---------------------------------------------------------------------------*/

inline size_t codepoint_length(std::string_view sv) {
    if (!sv.empty()) {
    auto b = static_cast<uint8_t>(sv.front());
    if ((b & 0x80) == 0) {
      return 1;
    } else if ((b & 0xE0) == 0xC0 && sv.size() >= 2) {
      return 2;
    } else if ((b & 0xF0) == 0xE0 && sv.size() >= 3) {
      return 3;
    } else if ((b & 0xF8) == 0xF0 && sv.size() >= 4) {
      return 4;
    }
  }
  return PARSE_ERROR;
}

Rule::Rule(std::string_view name, ContextProvider provider,
                           std::shared_ptr<GrammarElement> element,
                           std::function<bool(std::any &, CstNode &)> action,
                           DataType type)
    : _name{name}, _context_provider{std::move(provider)},
      _element(std::move(element)), _action{std::move(action)}, _type{type} {}

const std::string &Rule::name() const noexcept { return _name; }

ParseResult ParserRule::parse(std::string_view text) const {

  ParseResult result;
  result.root_node = std::make_shared<RootCstNode>();
  result.root_node->fullText = text;
  std::string_view sv = result.root_node->fullText;
  result.root_node->text = result.root_node->fullText;
  result.root_node->grammarSource = this;
  Context c = _context_provider();

  auto i = c.skipHiddenNodes(sv, *result.root_node);

  result.len =
      i + parse_rule({sv.data() + i, sv.size() - i}, *result.root_node, c);

  result.ret = result.len == sv.size();

  return result;
}
ParseResult TerminalRule::parse(std::string_view text) const {

  ParseResult result;
  result.root_node = std::make_shared<RootCstNode>();
  result.root_node->fullText = text;
  std::string_view sv = result.root_node->fullText;
  result.root_node->text = result.root_node->fullText;
  result.root_node->grammarSource = this;
  //Context c = _context_provider();


  result.len = parse_terminal(sv);

  result.ret = result.len == sv.size();

  return result;
}

RuleCall::RuleCall(const std::shared_ptr<Rule> &rule) : _rule(rule) {}

std::size_t RuleCall::parse_rule(std::string_view sv, CstNode &parent,
                                 Context &c) const {
  assert(_rule && "Call an undefined rule");

  CstNode node;
  auto i = _rule->parse_rule(sv, node, c);

  if (success(i)) {
    node.text = {sv.data(), i};
    node.grammarSource = _rule.get();
    parent.content.emplace_back(std::move(node));
  }
  return i;
}

std::size_t RuleCall::parse_hidden(std::string_view sv, CstNode &parent) const {
  assert(_rule && "Call an undefined rule");

  return _rule->parse_hidden(sv, parent);
}
std::size_t RuleCall::parse_terminal(std::string_view sv) const {
  assert(_rule && "Call an undefined rule");
  return _rule->parse_terminal(sv);
}
void RuleCall::accept(Visitor &v) const { v.visit(*this); }

ParserRule::ParserRule(std::string_view name, ContextProvider provider,
                       std::shared_ptr<GrammarElement> element,
                       std::function<bool(std::any &, CstNode &)> action,
                       DataType type)
    : Rule(name, std::move(provider), std::move(element),
                   std::move(action), type) {}

std::size_t Rule::parse_rule(std::string_view sv, CstNode &parent,
                                     Context &c) const {
  assert(_element);
  return _element->parse_rule(sv, parent, c);
}
std::size_t Rule::parse_hidden(std::string_view sv,
                                       CstNode &parent) const {
  assert(_element);
  return _element->parse_hidden(sv, parent);
}
std::size_t Rule::parse_terminal(std::string_view sv) const {
  assert(_element);
  return _element->parse_terminal(sv);
}
void ParserRule::accept(Visitor &v) const { v.visit(*this); }

TerminalRule::TerminalRule(std::string_view name, ContextProvider provider,
                           std::shared_ptr<GrammarElement> element,
                           TerminalRule::Kind kind,
                           std::function<bool(std::any &, CstNode &)> action,
                           DataType type)
    : Rule(name, std::move(provider), std::move(element),
                   std::move(action), type),
      _kind{kind} {}
std::size_t TerminalRule::parse_rule(std::string_view sv, CstNode &parent,
                                     Context &c) const {
  auto i = Rule::parse_terminal(sv);
  if (fail(i)) {
    return PARSE_ERROR;
  }
  // Do not create a node if the rule is ignored
  if (_kind != TerminalRule::Kind::Ignored) {
    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i};
    node.hidden = _kind == TerminalRule::Kind::Hidden;
    node.isLeaf = true;
  }
  return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
}

std::size_t TerminalRule::parse_hidden(std::string_view sv,
                                       CstNode &parent) const {
  auto i = Rule::parse_terminal(sv);
  if (fail(i)) {
    return PARSE_ERROR;
  }
  // Do not create a node if the rule is ignored
  if (_kind != TerminalRule::Kind::Ignored) {
    auto &node = parent.content.emplace_back();
    node.grammarSource = this;
    node.text = {sv.data(), i};
    node.hidden = true;
    node.isLeaf = true;
  }
  return i;
}

void TerminalRule::accept(Visitor &v) const { v.visit(*this); }
TerminalRule::Kind TerminalRule::getKind() const { return _kind; }

CharacterClass::CharacterClass(std::string_view s, bool negated,
                               bool ignore_case)
    : _name{s} {

  std::size_t i = 0;
  while (i < s.size()) {
    if (i + 2 < s.size() && s[i + 1] == '-') {
      for (auto c = s[i]; c <= s[i + 2]; ++c)
        lookup[static_cast<unsigned char>(c)] = true;
      i += 3;
    } else {
      lookup[static_cast<unsigned char>(s[i])] = true;
      i += 1;
    }
  }
  if (ignore_case) {
    for (auto c = 'a'; c <= 'z'; ++c)
      lookup[static_cast<unsigned char>(c)] |=
          lookup[static_cast<unsigned char>(c - 'a' + 'A')];
    for (auto c = 'A'; c <= 'Z'; ++c)
      lookup[static_cast<unsigned char>(c)] |=
          lookup[static_cast<unsigned char>(c - 'A' + 'a')];
  }
  if (negated) {
    for (auto &c : lookup)
      c = !c;
  }
}
std::size_t CharacterClass::parse_rule(std::string_view sv, CstNode &parent,
                                       Context &c) const {
  auto i = CharacterClass::parse_terminal(sv);
  if (fail(i)) {
    // c.set_error_pos(s);
    return PARSE_ERROR;
  }
  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.isLeaf = true;

  return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
}

std::size_t CharacterClass::parse_hidden(std::string_view sv,
                                         CstNode &parent) const {
  auto i = CharacterClass::parse_terminal(sv);
  if (fail(i)) {
    // c.set_error_pos(s);
    return PARSE_ERROR;
  }
  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.isLeaf = true;
  node.hidden = true;
  return i;
}

std::size_t CharacterClass::parse_terminal(std::string_view sv) const {
  return (!sv.empty() && lookup[static_cast<unsigned char>(sv[0])])
             ? 1
             : PARSE_ERROR;
}

void CharacterClass::accept(Visitor &v) const { v.visit(*this); }

std::size_t AnyCharacter::parse_rule(std::string_view sv, CstNode &parent,
                                     Context &c) const {
  auto i = codepoint_length(sv);
  if (fail(i)) {
    // c.set_error_pos(s);
    return PARSE_ERROR;
  }
  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.isLeaf = true;

  return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
}
std::size_t AnyCharacter::parse_hidden(std::string_view sv,
                                       CstNode &parent) const {
  auto i = codepoint_length(sv);
  if (fail(i)) {
    // c.set_error_pos(s);
    return PARSE_ERROR;
  }
  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.hidden = true;
  node.isLeaf = true;
  return i;
}
std::size_t AnyCharacter::parse_terminal(std::string_view sv) const {
  return codepoint_length(sv);
}

void AnyCharacter::accept(Visitor &v) const { v.visit(*this); }

NotPredicate::NotPredicate(std::shared_ptr<GrammarElement> element)
    : _element(std::move(element)) {}

std::size_t NotPredicate::parse_rule(std::string_view sv, CstNode &parent,
                                     Context &c) const {
  CstNode node;
  return success(_element->parse_rule(sv, node, c)) ? PARSE_ERROR : 0;
}
std::size_t NotPredicate::parse_hidden(std::string_view sv,
                                       CstNode &parent) const {
  CstNode node;
  return success(_element->parse_hidden(sv, node)) ? PARSE_ERROR : 0;
}
std::size_t NotPredicate::parse_terminal(std::string_view sv) const {
  return success(_element->parse_terminal(sv)) ? PARSE_ERROR : 0;
}

void NotPredicate::accept(Visitor &v) const { v.visit(*this); }

AndPredicate::AndPredicate(std::shared_ptr<GrammarElement> element)
    : _element(std::move(element)) {}

std::size_t AndPredicate::parse_rule(std::string_view sv, CstNode &parent,
                                     Context &c) const {
  CstNode node;
  return success(_element->parse_rule(sv, node, c)) ? 0 : PARSE_ERROR;
}
std::size_t AndPredicate::parse_hidden(std::string_view sv,
                                       CstNode &parent) const {
  CstNode node;
  return success(_element->parse_hidden(sv, node)) ? 0 : PARSE_ERROR;
}

std::size_t AndPredicate::parse_terminal(std::string_view sv) const {
  return success(_element->parse_terminal(sv)) ? 0 : PARSE_ERROR;
}

void AndPredicate::accept(Visitor &v) const { v.visit(*this); }

PrioritizedChoice::PrioritizedChoice(
    std::vector<std::shared_ptr<GrammarElement>> &&elements)
    : _elements{std::move(elements)} {}

std::size_t PrioritizedChoice::parse_rule(std::string_view sv, CstNode &parent,
                                          Context &c) const {

  const auto size = parent.content.size();

  for (const auto &elem : _elements) {
    if (auto i = elem->parse_rule(sv, parent, c); success(i)) {
      return i;
    }
    parent.content.resize(size);
  }

  return PARSE_ERROR;
}
std::size_t PrioritizedChoice::parse_hidden(std::string_view sv,
                                            CstNode &parent) const {
  auto size = parent.content.size();

  for (const auto &elem : _elements) {
    if (auto i = elem->parse_hidden(sv, parent); success(i)) {
      return i;
    }
    parent.content.resize(size);
  }

  return PARSE_ERROR;
}

std::size_t PrioritizedChoice::parse_terminal(std::string_view sv) const {
  for (const auto &elem : _elements) {
    if (auto i = elem->parse_terminal(sv); success(i)) {
      return i;
    }
  }
  return PARSE_ERROR;
}

void PrioritizedChoice::accept(Visitor &v) const { v.visit(*this); }

std::shared_ptr<PrioritizedChoice> &
operator|(std::shared_ptr<PrioritizedChoice> &lhs,
          std::shared_ptr<GrammarElement> rhs) {
  lhs->_elements.emplace_back(std::move(rhs));
  return lhs;
}

std::shared_ptr<PrioritizedChoice>
operator|(std::shared_ptr<GrammarElement> lhs,
          std::shared_ptr<GrammarElement> rhs) {
  return std::make_shared<PrioritizedChoice>(std::move(lhs), std::move(rhs));
}

Repetition::Repetition(std::shared_ptr<GrammarElement> element, size_t min,
                       size_t max)
    : _element(std::move(element)), _min(min), _max(max) {}

std::size_t Repetition::parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const {
  std::size_t count = 0;
  std::size_t i = 0;
  auto size = parent.content.size();

  while (count < _min) {

    auto len = _element->parse_rule({sv.data() + i, sv.size() - i}, parent, c);

    if (fail(len)) {
      parent.content.resize(size);

      return len;
    }
    i += len;
    count++;
  }

  while (count < _max) {
    size = parent.content.size();
    auto len = _element->parse_rule({sv.data() + i, sv.size() - i}, parent, c);

    if (fail(len)) {
      parent.content.resize(size);

      break;
    }
    i += len;
    count++;
  }

  return i;
}
std::size_t Repetition::parse_hidden(std::string_view sv,
                                     CstNode &parent) const {
  std::size_t count = 0;
  std::size_t i = 0;
  auto size = parent.content.size();

  while (count < _min) {

    auto len = _element->parse_hidden({sv.data() + i, sv.size() - i}, parent);

    if (fail(len)) {
      parent.content.resize(size);

      return len;
    }
    i += len;
    count++;
  }

  while (count < _max) {
    size = parent.content.size();
    auto len = _element->parse_hidden({sv.data() + i, sv.size() - i}, parent);

    if (fail(len)) {
      parent.content.resize(size);

      break;
    }
    i += len;
    count++;
  }

  return i;
}

std::size_t Repetition::parse_terminal(std::string_view sv) const {
  std::size_t count = 0;
  std::size_t i = 0;
  while (count < _min) {

    auto len = _element->parse_terminal({sv.data() + i, sv.size() - i});

    if (fail(len)) {
      return len;
    }
    i += len;
    count++;
  }

  while (count < _max) {

    auto len = _element->parse_terminal({sv.data() + i, sv.size() - i});

    if (fail(len)) {
      break;
    }
    i += len;
    count++;
  }

  return i;
}
void Repetition::accept(Visitor &v) const { v.visit(*this); }

bool Repetition::is_many() const {
  return _min == 0 && _max == std::numeric_limits<size_t>::max();
}

Optional::Optional(std::shared_ptr<GrammarElement> element)
    : _element{std::move(element)} {}
std::size_t Optional::parse_rule(std::string_view sv, CstNode &parent,
                                 Context &c) const {
  auto size = parent.content.size();
  auto i = _element->parse_rule(sv, parent, c);
  if (fail(i)) {
    parent.content.resize(size);
    return 0;
  }
  return i;
}

std::size_t Optional::parse_hidden(std::string_view sv, CstNode &parent) const {
  auto size = parent.content.size();
  auto i = _element->parse_hidden(sv, parent);
  if (fail(i)) {
    parent.content.resize(size);
    return 0;
  }
  return i;
}
std::size_t Optional::parse_terminal(std::string_view sv) const {
  auto i = _element->parse_terminal(sv);
  return fail(i) ? 0 : i;
}

Many::Many(std::shared_ptr<GrammarElement> element)
    : _element{std::move(element)} {}
std::size_t Many::parse_rule(std::string_view sv, CstNode &parent,
                             Context &c) const {
  std::size_t i = 0;
  while (true) {
    auto size = parent.content.size();
    auto len = _element->parse_rule({sv.data() + i, sv.size() - i}, parent, c);

    if (fail(len)) {
      parent.content.resize(size);
      break;
    }
    i += len;
  }
  return i;
}

std::size_t Many::parse_hidden(std::string_view sv, CstNode &parent) const {
  std::size_t i = 0;
  while (true) {
    auto size = parent.content.size();
    auto len = _element->parse_hidden({sv.data() + i, sv.size() - i}, parent);

    if (fail(len)) {
      parent.content.resize(size);
      break;
    }
    i += len;
  }
  return i;
}
std::size_t Many::parse_terminal(std::string_view sv) const {
  std::size_t i = 0;
  while (true) {
    auto len = _element->parse_terminal({sv.data() + i, sv.size() - i});
    if (fail(len)) {
      break;
    }
    i += len;
  }

  return i;
}

AtLeastOne::AtLeastOne(std::shared_ptr<GrammarElement> element)
    : _element{std::move(element)} {}
std::size_t AtLeastOne::parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const {

  auto size = parent.content.size();
  auto i = _element->parse_rule(sv, parent, c);
  if (fail(i)) {
    parent.content.resize(size);
    return i;
  }
  while (true) {
    size = parent.content.size();
    auto len = _element->parse_rule({sv.data() + i, sv.size() - i}, parent, c);

    if (fail(len)) {
      parent.content.resize(size);
      break;
    }
    i += len;
  }
  return i;
}

std::size_t AtLeastOne::parse_hidden(std::string_view sv,
                                     CstNode &parent) const {
  auto size = parent.content.size();
  auto i = _element->parse_hidden(sv, parent);
  if (fail(i)) {
    parent.content.resize(size);
    return i;
  }
  while (true) {
    size = parent.content.size();
    auto len = _element->parse_hidden({sv.data() + i, sv.size() - i}, parent);

    if (fail(len)) {
      parent.content.resize(size);
      break;
    }
    i += len;
  }
  return i;
}
std::size_t AtLeastOne::parse_terminal(std::string_view sv) const {

  auto i = _element->parse_terminal(sv);
  if (fail(i))
    return i;

  while (true) {
    auto len = _element->parse_terminal({sv.data() + i, sv.size() - i});
    if (fail(len)) {
      break;
    }
    i += len;
  }

  return i;
}

Group::Group(std::shared_ptr<Group> seq) noexcept
    : _elements{std::move(seq->_elements)} {}

std::size_t Group::parse_rule(std::string_view sv, CstNode &parent,
                              Context &c) const {

  size_t i = 0;
  auto size = parent.content.size();

  for (const auto &element : _elements) {
    auto len = element->parse_rule({sv.data() + i, sv.size() - i}, parent, c);
    if (fail(len)) {
      parent.content.resize(size);

      return len;
    }
    i += len;
  }

  return i;
}
std::size_t Group::parse_hidden(std::string_view sv, CstNode &parent) const {
  size_t i = 0;
  for (const auto &element : _elements) {
    auto len = element->parse_hidden({sv.data() + i, sv.size() - i}, parent);
    if (fail(len)) {
      return len;
    }
    i += len;
  }

  return i;
}

std::size_t Group::parse_terminal(std::string_view sv) const {

  size_t i = 0;
  for (const auto &element : _elements) {
    auto len = element->parse_terminal({sv.data() + i, sv.size() - i});
    if (fail(len)) {
      return len;
    }
    i += len;
  }

  return i;
}

void Group::accept(Visitor &v) const { v.visit(*this); }

std::shared_ptr<Group> &operator,(std::shared_ptr<Group> &lhs,
                                  std::shared_ptr<GrammarElement> rhs) {
  lhs->_elements.emplace_back(std::move(rhs));
  return lhs;
}
std::shared_ptr<Group> operator,(std::shared_ptr<GrammarElement> lhs,
                                 std::shared_ptr<GrammarElement> rhs) {
  return std::make_shared<Group>(std::move(lhs), std::move(rhs));
}

Keyword::Keyword(std::string s, bool ignore_case)
    : _kw(std::move(s)), _ignore_case{ignore_case} {}

std::size_t Keyword::parse_rule(std::string_view sv, CstNode &parent,
                                      Context &c) const {
  auto i = Keyword::parse_terminal(sv);
  if (fail(i) || (isword(_kw.back()) && isword(sv[i])))
    return PARSE_ERROR;

  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.isLeaf = true;

  return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
}
std::size_t Keyword::parse_hidden(std::string_view sv,
                                        CstNode &parent) const {
  auto i = Keyword::parse_terminal(sv);
  if (fail(i) || (isword(_kw.back()) && isword(sv[i])))
    return PARSE_ERROR;

  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.hidden = true;
  node.isLeaf = true;
  return i;
}

std::size_t Keyword::parse_terminal(std::string_view sv) const {
  if (_kw.size() > sv.size())
    return PARSE_ERROR;
  std::size_t i = 0;
  for (; i < _kw.size(); i++) {

    if (_ignore_case ? (tolower(sv[i]) != tolower(_kw[i]))
                     : (sv[i] != _kw[i])) {
      // c.set_error_pos(s, lit_.data());
      return PARSE_ERROR;
    }
  }
  return i;
}

void Keyword::accept(Visitor &v) const { v.visit(*this); }

Character::Character(char c) : _char(c) {}

std::size_t Character::parse_rule(std::string_view sv, CstNode &parent,
                                  Context &c) const {

  auto i = Character::parse_terminal(sv);
  if (fail(i) || (isword(_char) && sv.size() > i && isword(sv[i])))
    return PARSE_ERROR;

  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.isLeaf = true;
  return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
}

std::size_t Character::parse_hidden(std::string_view sv,
                                    CstNode &parent) const {
  auto i = Character::parse_terminal(sv);
  if (fail(i) || (isword(_char) && isword(sv[i])))
    return PARSE_ERROR;

  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  node.text = {sv.data(), i};
  node.hidden = true;
  node.isLeaf = true;
  return i;
}

std::size_t Character::parse_terminal(std::string_view sv) const {
  // TODO handle word boundary
  return sv.empty() || _char != sv[0] ? PARSE_ERROR : 1;
}

void Character::accept(Visitor &v) const { v.visit(*this); }

std::size_t Action::parse_rule(std::string_view sv, CstNode &parent,
                               Context &c) const {
  auto &node = parent.content.emplace_back();
  node.grammarSource = this;
  return 0;
}
std::size_t Action::parse_terminal(std::string_view sv) const { return 0; }
std::size_t Action::parse_hidden(std::string_view sv, CstNode &parent) const {
  return 0;
}
void Action::accept(Visitor &v) const { v.visit(*this); }

Assignment::Assignment(Feature feature, std::shared_ptr<GrammarElement> elem)
    : feature{std::move(feature)}, elem{std::move(elem)} {}
std::size_t Assignment::parse_rule(std::string_view sv, CstNode &parent,
                                   Context &c) const {
  CstNode node;
  auto i = elem->parse_rule(sv, node, c);
  if (success(i)) {
    node.text = {sv.data(), i};
    node.grammarSource = this;
    parent.content.emplace_back(std::move(node));
  }
  return i;
}
std::size_t Assignment::parse_terminal(std::string_view sv) const {
  assert(false && "An Assignment cannot be in a terminal");
  return PARSE_ERROR;
}
std::size_t Assignment::parse_hidden(std::string_view sv,
                                     CstNode &parent) const {
  assert(false && "An Assignment cannot be hidden");
  return PARSE_ERROR;
}

void Assignment::accept(Visitor &v) const { v.visit(*this); }

Context::Context(std::shared_ptr<GrammarElement> hidden)
    : hidden{std::move(hidden)} {}

std::size_t Context::skipHiddenNodes(std::string_view sv, CstNode &node) const {

  std::size_t i = 0;
  auto len = hidden->parse_hidden(sv, node);
  while (len != PARSE_ERROR) {
    assert(len && "An hidden rule must consume at least one character.");
    i += len;
    len = hidden->parse_hidden({sv.data() + i, sv.size() - i}, node);
  }
  return i;
}

std::size_t NoOp::parse_rule(std::string_view sv, CstNode &parent,
                             Context &c) const {
  return PARSE_ERROR;
}
std::size_t NoOp::parse_terminal(std::string_view sv) const {
  return PARSE_ERROR;
}

std::size_t NoOp::parse_hidden(std::string_view sv, CstNode &parent) const {
  return PARSE_ERROR;
}
void NoOp::accept(Visitor &v) const {}
std::shared_ptr<GrammarElement> operator"" _kw(const char *str, std::size_t s) {
  return std::make_shared<Keyword>(std::string(str, s));
}
std::shared_ptr<GrammarElement> operator"" _ikw(const char *str,
                                                std::size_t s) {
  return std::make_shared<Keyword>(std::string(str, s), true);
}

std::shared_ptr<Character> operator"" _kw(char chr) {
  return std::make_shared<Character>(chr);
}

/*std::shared_ptr<Character> operator"" _ikw(char chr) {
  return std::make_shared<Character>(chr);
}*/

bool Feature::operator==(const Feature &rhs) const noexcept {
  return _equal(*this, rhs);
}
void Feature::assign(const std::any &object, const std::any &value) const {
  _assign(std::any_cast<std::shared_ptr<pegium::AstNode>>(object).get(),
          _feature, value);
}
void Feature::assign(AstNode *object, const std::any &value) const {
  _assign(object, _feature, value);
}

std::shared_ptr<Assignment>
Feature::operator=(std::shared_ptr<GrammarElement> elem) {
  return std::make_shared<Assignment>(*this, elem);
}

} // namespace pegium