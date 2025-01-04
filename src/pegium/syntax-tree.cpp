#include <pegium/grammar/IGrammarElement.hpp>
#include <pegium/syntax-tree.hpp>
namespace pegium {

std::ostream &operator<<(std::ostream &os, const CstNode &obj) {
  os << "{\n";
  if (obj.grammarSource)
    os << "\"grammarSource\": \"" << *obj.grammarSource << "\",\n";

  if (obj.isLeaf())
    os << "\"text\": \"" << obj.text << "\",\n";

  if (obj.hidden)
    os << "\"hidden\": true,\n";
  if (!obj.content.empty()) {

    os << "\"content\": [\n";
    for (auto n : obj.content)
      os << n;
    os << "],\n";
  }

  os << "},\n";
  return os;
}
} // namespace pegium