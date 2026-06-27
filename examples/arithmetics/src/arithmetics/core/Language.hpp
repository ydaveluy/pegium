#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <arithmetics/core/ast.hpp>

namespace arithmetics {

std::string canonical_identifier(std::string_view text);

using ContextValue = std::variant<double, const ast::Definition *>;
using EvaluationResult = std::unordered_map<const ast::Evaluation *, double>;

EvaluationResult interpret_evaluations(ast::Module &module);
std::vector<double> evaluate_module(ast::Module &module);

} // namespace arithmetics
