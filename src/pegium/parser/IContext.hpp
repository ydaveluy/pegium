#pragma once
#include <pegium/syntax-tree.hpp>
#include <string_view>

namespace pegium::parser {
struct MatchResult;
struct IContext {

  virtual MatchResult skipHiddenNodes(std::string_view sv,
                                      pegium::CstNode &node) const = 0;
  virtual ~IContext() noexcept = default;

  virtual void addRecovery(std::string_view expected, std::string_view position) = 0;
  virtual void clearRecovery() = 0;
  virtual void setInputText(std::string_view) = 0;
};

} // namespace pegium::parser