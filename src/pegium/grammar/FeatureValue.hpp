#pragma once

#include <variant>
#include <vector>

#include <pegium/grammar/RuleValue.hpp>

namespace pegium {

class AbstractReference;
struct AstNode;

} // namespace pegium

namespace pegium::grammar {

class FeatureValue {
public:
  struct ReferenceValue {
    const AbstractReference *value = nullptr;
  };

  using Array = std::vector<FeatureValue>;
  using Storage = std::variant<RuleValue, const AstNode *, ReferenceValue, Array>;

  FeatureValue() noexcept;
  explicit FeatureValue(RuleValue value) noexcept;
  explicit FeatureValue(const AstNode *value) noexcept;
  explicit FeatureValue(const AbstractReference *value) noexcept;
  explicit FeatureValue(ReferenceValue value) noexcept;
  explicit FeatureValue(Array value) noexcept;

  [[nodiscard]] bool isRuleValue() const noexcept;
  [[nodiscard]] bool isAstNode() const noexcept;
  [[nodiscard]] bool isReference() const noexcept;
  [[nodiscard]] bool isArray() const noexcept;

  [[nodiscard]] const RuleValue &ruleValue() const;
  [[nodiscard]] const AstNode *astNode() const;
  [[nodiscard]] const ReferenceValue &reference() const;
  [[nodiscard]] const Array &array() const;

private:
  Storage _value;
};

} // namespace pegium::grammar
