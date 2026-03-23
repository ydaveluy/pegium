#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium::validation::test_support {

struct ValidationNodeA : pegium::AstNode {};
struct ValidationNodeB : pegium::AstNode {};
struct ValidationRefNode : pegium::AstNode {
  reference<ValidationNodeA> ref;
};

} // namespace pegium::validation::test_support
