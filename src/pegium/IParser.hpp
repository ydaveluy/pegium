#pragma once

#include <any>
#include <memory>
#include <pegium/syntax-tree.hpp>
#include <string>
#include <string_view>

namespace pegium {

struct ParseResult {
  bool ret;
  bool recovered;
  size_t len;
  std::shared_ptr<RootCstNode> root_node;
  std::any value;
  // TODO add errors
};

class IParser {
public:
  virtual ~IParser() noexcept = default;


   virtual ParseResult parse(const std::string &input) const = 0;
};

} // namespace pegium