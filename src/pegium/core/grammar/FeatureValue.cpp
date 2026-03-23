#include <pegium/core/grammar/FeatureValue.hpp>

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium::grammar {

FeatureValue::FeatureValue() noexcept : _value(RuleValue{nullptr}) {}

FeatureValue::FeatureValue(RuleValue value) noexcept : _value(std::move(value)) {}

FeatureValue::FeatureValue(const AstNode *value) noexcept : _value(value) {}

FeatureValue::FeatureValue(const AbstractReference *value) noexcept
    : _value(ReferenceValue{.value = value}) {}

FeatureValue::FeatureValue(ReferenceValue value) noexcept : _value(value) {}

FeatureValue::FeatureValue(Array value) noexcept : _value(std::move(value)) {}

bool FeatureValue::isRuleValue() const noexcept {
  return std::holds_alternative<RuleValue>(_value);
}

bool FeatureValue::isAstNode() const noexcept {
  return std::holds_alternative<const AstNode *>(_value);
}

bool FeatureValue::isReference() const noexcept {
  return std::holds_alternative<ReferenceValue>(_value);
}

bool FeatureValue::isArray() const noexcept {
  return std::holds_alternative<Array>(_value);
}

const RuleValue &FeatureValue::ruleValue() const {
  return std::get<RuleValue>(_value);
}

const AstNode *FeatureValue::astNode() const {
  return std::get<const AstNode *>(_value);
}

const FeatureValue::ReferenceValue &FeatureValue::reference() const {
  return std::get<ReferenceValue>(_value);
}

const FeatureValue::Array &FeatureValue::array() const {
  return std::get<Array>(_value);
}

} // namespace pegium::grammar
