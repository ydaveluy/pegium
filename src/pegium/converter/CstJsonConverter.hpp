#pragma once

#include <pegium/services/JsonValue.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::converter {

struct CstJsonConversionOptions {
  bool includeText = true;
  bool includeGrammarSource = true;
  bool includeHidden = true;
  bool includeRecovered = true;
};

class CstJsonConverter {
public:
  using Options = CstJsonConversionOptions;

  [[nodiscard]] static services::JsonValue convert(const CstNodeView &node,
                                                   const Options &options = {});
  [[nodiscard]] static services::JsonValue convert(const RootCstNode &root,
                                                   const Options &options = {});
};

} // namespace pegium::converter
