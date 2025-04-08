
#include <pegium/syntax-tree.hpp>
#include <ostream>
namespace pegium {

std::ostream &operator<<(std::ostream &os, const CstNode &obj) {
  os << "{\n";
  // if (obj.grammarSource)
  //  os << "\"grammarSource\": \"" << *obj.grammarSource << "\",\n";

  if (obj.isLeaf()) {
    os << "\"text\": \"" << obj.text << "\",\n";
  }

  if (obj.hidden) {
    os << "\"hidden\": true,\n";
  }
  if (!obj.content.empty()) {

    os << "\"content\": [\n";
    for (const auto &n : obj.content) {
      os << n;
    }
    os << "],\n";
  }

  os << "},\n";
  return os;
}
} // namespace pegium