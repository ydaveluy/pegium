#pragma once

#include <pegium/services/JsonValue.hpp>
#include <pegium/syntax-tree/AstNode.hpp>

namespace pegium::converter {

struct AstJsonConversionOptions {
  bool includeType = true;
  bool includeReferenceText = true;
  bool includeReferenceErrors = true;
};

class AstJsonConverter {
public:
  using Options = AstJsonConversionOptions;

  [[nodiscard]] static services::JsonValue convert(const AstNode &node,
                                                   const Options &options = {});
};

} // namespace pegium::converter
