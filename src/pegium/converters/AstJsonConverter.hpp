#pragma once

#include <pegium/core/services/JsonValue.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium::converter {

/// Controls which AST metadata is emitted in JSON output.
struct AstJsonConversionOptions {
  bool includeType = true;
  bool includeReferenceText = true;
  bool includeReferenceErrors = true;
};

/// Converts AST nodes to the shared JSON representation.
class AstJsonConverter {
public:
  using Options = AstJsonConversionOptions;

  /// Converts `node` to JSON using `options`.
  [[nodiscard]] static pegium::JsonValue convert(const AstNode &node,
                                                   const Options &options = {});
};

} // namespace pegium::converter
