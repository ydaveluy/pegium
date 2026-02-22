#include <pegium/syntax-tree/Json.hpp>

#include <sstream>

namespace pegium {

namespace {

void appendIndent(std::string &out, std::size_t count) { out.append(count, ' '); }

void appendEscapedJsonString(std::string &out, std::string_view text) {
  static constexpr char hex[] = "0123456789ABCDEF";
  out.push_back('"');
  for (const unsigned char c : text) {
    switch (c) {
    case '\"':
      out += R"(\")";
      break;
    case '\\':
      out += R"(\\)";
      break;
    case '\b':
      out += R"(\b)";
      break;
    case '\f':
      out += R"(\f)";
      break;
    case '\n':
      out += R"(\n)";
      break;
    case '\r':
      out += R"(\r)";
      break;
    case '\t':
      out += R"(\t)";
      break;
    default:
      if (c < 0x20) {
        out += "\\u00";
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0x0F]);
      } else {
        out.push_back(static_cast<char>(c));
      }
      break;
    }
  }
  out.push_back('"');
}

void appendJsonFieldName(std::string &out, std::string_view name,
                         std::size_t indent) {
  appendIndent(out, indent);
  appendEscapedJsonString(out, name);
  out += ": ";
}

void appendGrammarSource(std::string &out,
                         const grammar::AbstractElement *grammarElement) {
  if (!grammarElement) {
    out += "null";
    return;
  }
  std::ostringstream ss;
  ss << *grammarElement;
  appendEscapedJsonString(out, ss.str());
}

void appendNodeAsJson(std::string &out, const CstNodeView &node,
                      std::size_t currentIndent, std::size_t indentStep);

void appendChildrenAsJson(std::string &out, const CstNodeView &node,
                          std::size_t currentIndent, std::size_t indentStep) {
  out += "[\n";
  bool first = true;
  for (const auto &child : node) {
    if (!first) {
      out += ",\n";
    }
    first = false;
    appendIndent(out, currentIndent + indentStep);
    appendNodeAsJson(out, child, currentIndent + indentStep, indentStep);
  }
  out += '\n';
  appendIndent(out, currentIndent);
  out += "]";
}

void appendNodeAsJson(std::string &out, const CstNodeView &node,
                      std::size_t currentIndent, std::size_t indentStep) {
  out += "{\n";
  bool hasField = false;
  auto appendComma = [&]() {
    if (hasField) {
      out += ",\n";
    }
    hasField = true;
  };

  appendComma();
  appendJsonFieldName(out, "begin", currentIndent + indentStep);
  out += std::to_string(node.getBegin());

  appendComma();
  appendJsonFieldName(out, "end", currentIndent + indentStep);
  out += std::to_string(node.getEnd());

  appendComma();
  appendJsonFieldName(out, "text", currentIndent + indentStep);
  appendEscapedJsonString(out, node.getText());

  appendComma();
  appendJsonFieldName(out, "grammarSource", currentIndent + indentStep);
  appendGrammarSource(out, node.getGrammarElement());

  if (node.isHidden()) {
    appendComma();
    appendJsonFieldName(out, "hidden", currentIndent + indentStep);
    out += "true";
  }

  if (!node.isLeaf()) {
    appendComma();
    appendJsonFieldName(out, "content", currentIndent + indentStep);
    appendChildrenAsJson(out, node, currentIndent + indentStep, indentStep);
  }

  out += '\n';
  appendIndent(out, currentIndent);
  out += "}";
}

} // namespace

std::string toJson(const CstNode &node) {
  std::string out;
  out.reserve(128);
  out += "{\n";
  appendJsonFieldName(out, "begin", 2);
  out += std::to_string(node.begin);
  out += ",\n";
  appendJsonFieldName(out, "end", 2);
  out += std::to_string(node.end);
  out += ",\n";
  appendJsonFieldName(out, "grammarSource", 2);
  appendGrammarSource(out, node.grammarElement);
  if (node.isHidden) {
    out += ",\n";
    appendJsonFieldName(out, "hidden", 2);
    out += "true";
  }
  out += '\n';
  out += "}";
  return out;
}

std::string toJson(const RootCstNode &node) {
  std::string out;
  const auto text = node.getText();
  out.reserve(128 + text.size());
  out += "{\n";
  appendJsonFieldName(out, "fullText", 2);
  appendEscapedJsonString(out, text);
  out += ",\n";
  appendJsonFieldName(out, "content", 2);
  out += "[\n";
  bool first = true;
  for (const auto &child : node) {
    if (!first) {
      out += ",\n";
    }
    first = false;
    appendIndent(out, 4);
    appendNodeAsJson(out, child, 4, 2);
  }
  out += '\n';
  appendIndent(out, 2);
  out += "]";
  out += '\n';
  out += "}";
  return out;
}

std::ostream &operator<<(std::ostream &os, const CstNode &obj) {
  os << toJson(obj);
  return os;
}

} // namespace pegium
