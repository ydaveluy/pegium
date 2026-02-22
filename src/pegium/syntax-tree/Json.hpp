#pragma once

#include <ostream>
#include <string>

#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium {

/// Serialize a CST node to a JSON string.
std::string toJson(const CstNode &node);

/// Serialize a root CST node to a JSON string (including `fullText`).
std::string toJson(const RootCstNode &node);

std::ostream &operator<<(std::ostream &os, const CstNode &obj);

} // namespace pegium
