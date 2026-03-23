#pragma once

#include <pegium/core/services/JsonValue.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::converter {

/// Controls which CST metadata is emitted in JSON output.
struct CstJsonConversionOptions {
  bool includeText = true;
  bool includeGrammarSource = true;
  bool includeHidden = true;
  bool includeRecovered = true;
};

/// Converts CST nodes to the shared JSON representation.
class CstJsonConverter {
public:
  using Options = CstJsonConversionOptions;

  /// Converts one CST subtree to JSON using `options`.
  [[nodiscard]] static services::JsonValue convert(const CstNodeView &node,
                                                   const Options &options = {});
  /// Converts one CST root node to JSON using `options`.
  [[nodiscard]] static services::JsonValue convert(const RootCstNode &root,
                                                   const Options &options = {});
};

} // namespace pegium::converter
