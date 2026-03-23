#pragma once

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace arithmetics {

using BinaryOperator = double (*)(double, double);

inline double add_operator(double left, double right) noexcept {
  return left + right;
}

inline double subtract_operator(double left, double right) noexcept {
  return left - right;
}

inline double multiply_operator(double left, double right) noexcept {
  return left * right;
}

inline double exponent_operator(double left, double right) {
  return std::pow(left, right);
}

inline double modulo_operator(double left, double right) {
  return std::fmod(left, right);
}

inline double divide_operator(double left, double right) {
  if (right == 0.0) {
    throw std::runtime_error("Division by zero");
  }
  return left / right;
}

inline BinaryOperator apply_operator(std::string_view op) {
  if (op == "+") {
    return &add_operator;
  }
  if (op == "-") {
    return &subtract_operator;
  }
  if (op == "*") {
    return &multiply_operator;
  }
  if (op == "^") {
    return &exponent_operator;
  }
  if (op == "%") {
    return &modulo_operator;
  }
  if (op == "/") {
    return &divide_operator;
  }
  throw std::runtime_error("Unknown operator: " + std::string(op));
}

} // namespace arithmetics
