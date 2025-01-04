#pragma once
#include <pegium/syntax-tree.hpp>
#include <string_view>

namespace pegium::grammar {

struct IContext {

  virtual std::size_t skipHiddenNodes(std::string_view sv,
                                      pegium::CstNode &node) const = 0;
  virtual ~IContext() noexcept = default;
};

} // namespace pegium::grammar