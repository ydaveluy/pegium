#pragma once

#include <any>
#include <memory>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree/RootCstNode.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::parser {
enum class ParseDiagnosticKind {
  Inserted,
  Deleted,
  Replaced,
};

struct ParseDiagnostic {
  ParseDiagnosticKind kind = ParseDiagnosticKind::Deleted;
  size_t offset = 0;
  const grammar::AbstractElement *element = nullptr;
};

struct ParseOptions {
  std::size_t maxConsecutiveCodepointDeletes = 8;
  // 0 disables local-window limitation (legacy global recovery behavior).
  std::size_t localRecoveryWindowBytes = 128;
};

template <typename T> struct ParseResult {
  bool ret = false;
  bool recovered = false;
  size_t len = 0;
  std::shared_ptr<RootCstNode> root_node;
  T value;
  std::vector<ParseDiagnostic> diagnostics;
};

using GenericParseResult = ParseResult<std::any>;

class IParser {
public:
  virtual ~IParser() noexcept = default;
};

} // namespace pegium::parser
