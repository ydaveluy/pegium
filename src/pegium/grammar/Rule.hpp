#pragma once
#include <any>
#include <concepts>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree.hpp>

namespace pegium::grammar {

struct Rule : AbstractElement {
  virtual std::any getAnyValue(const CstNode &node) const = 0;
  constexpr ~Rule() noexcept override = default;

  virtual const AbstractElement* getElement()const noexcept = 0;
};
template <typename T>
concept IsRule = std::derived_from<std::remove_cvref_t<T>, Rule>;

} // namespace pegium::grammar