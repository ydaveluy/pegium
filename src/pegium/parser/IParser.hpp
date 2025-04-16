#pragma once

#include <any>
#include <memory>
#include <pegium/parser/IContext.hpp>
#include <pegium/syntax-tree.hpp>
#include <string>
#include <string_view>

namespace pegium::parser {
template <typename T> struct ParseResult {
  bool ret = false;
  bool recovered = false;
  size_t len = 0;
  std::shared_ptr<RootCstNode> root_node;
  T value;
  // TODO add errors
};

using GenericParseResult = ParseResult<std::any>;

class IParser {
public:
  virtual ~IParser() noexcept = default;
  virtual std::unique_ptr<IContext> createContext() const = 0;
  // virtual GenericParseResult parse(const std::string &input) const = 0;
};

} // namespace pegium::parser