#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <pegium/services/JsonValue.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/validation/DiagnosticRanges.hpp>

namespace pegium::validation {

class ValidationAcceptor;
template <typename Node> class ValidationDiagnosticBuilder;

template <typename Derived> class ValidationDiagnosticBuilderBase {
public:
  ValidationDiagnosticBuilderBase(const ValidationAcceptor &acceptor,
                                  services::DiagnosticSeverity severity,
                                  std::string_view message,
                                  const AstNode &node) noexcept;

  ValidationDiagnosticBuilderBase(const ValidationDiagnosticBuilderBase &) =
      delete;
  ValidationDiagnosticBuilderBase &
  operator=(const ValidationDiagnosticBuilderBase &) = delete;

  ValidationDiagnosticBuilderBase(
      ValidationDiagnosticBuilderBase &&other) noexcept
      : _acceptor(other._acceptor), _node(other._node),
        _severity(other._severity), _message(std::move(other._message)),
        _code(std::move(other._code)),
        _codeDescription(std::move(other._codeDescription)),
        _tags(std::move(other._tags)),
        _relatedInformation(std::move(other._relatedInformation)),
        _data(std::move(other._data)), _begin(other._begin), _end(other._end),
        _pending(other._pending) {
    other._acceptor = nullptr;
    other._pending = false;
  }

  ValidationDiagnosticBuilderBase &
  operator=(ValidationDiagnosticBuilderBase &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    if (_pending) {
      emit();
    }

    _acceptor = other._acceptor;
    _node = other._node;
    _severity = other._severity;
    _message = std::move(other._message);
    _code = std::move(other._code);
    _codeDescription = std::move(other._codeDescription);
    _tags = std::move(other._tags);
    _relatedInformation = std::move(other._relatedInformation);
    _data = std::move(other._data);
    _begin = other._begin;
    _end = other._end;
    _pending = other._pending;

    other._acceptor = nullptr;
    other._pending = false;
    return *this;
  }

  ~ValidationDiagnosticBuilderBase() {
    if (_pending) {
      emit();
    }
  }

  Derived &range(TextOffset begin, TextOffset end) noexcept {
    const auto baseBegin = _begin;
    const auto baseEnd = _end;
    const auto selectedBegin = std::min(baseBegin + begin, baseEnd);
    const auto selectedEnd = std::min(baseBegin + end, baseEnd);
    _begin = selectedBegin;
    _end = std::max(selectedBegin, selectedEnd);
    return derived();
  }

  Derived &code(std::string_view codeValue,
                std::optional<std::string_view> description =
                    std::nullopt) {
    _code = services::DiagnosticCode(std::string(codeValue));
    _codeDescription =
        description.has_value() ? std::optional<std::string>(std::string(*description))
                                : std::nullopt;
    return derived();
  }

  Derived &tags(std::initializer_list<services::DiagnosticTag> tags) {
    _tags.assign(tags.begin(), tags.end());
    return derived();
  }

  Derived &relatedInformation(TextOffset begin, TextOffset end,
                                  std::string_view message,
                                  std::string_view uri = {}) {
    _relatedInformation.push_back(services::DiagnosticRelatedInformation{
        .uri = std::string(uri),
        .message = std::string(message),
        .begin = begin,
        .end = std::max(begin, end)});
    return derived();
  }

  Derived &relatedInformation(const AstNode &node, std::string_view message,
                                  std::string_view uri = {}) {
    const auto [begin, end] = range_of(node);
    return relatedInformation(begin, end, message, uri);
  }

  Derived &relatedInformation(
      std::initializer_list<services::DiagnosticRelatedInformation> entries) {
    _relatedInformation.insert(_relatedInformation.end(), entries.begin(),
                               entries.end());
    return derived();
  }

  Derived &relatedInformation(
      std::span<const services::DiagnosticRelatedInformation> entries) {
    _relatedInformation.insert(_relatedInformation.end(), entries.begin(),
                               entries.end());
    return derived();
  }

  Derived &data(services::JsonValue dataValue) {
    _data = std::move(dataValue);
    return derived();
  }


protected:
  void emit();

  [[nodiscard]] const AstNode *node() const noexcept { return _node; }

  void setRange(std::pair<TextOffset, TextOffset> range) noexcept {
    std::tie(_begin, _end) = range;
  }

private:
  Derived &derived() noexcept { return static_cast<Derived &>(*this); }

  const ValidationAcceptor *_acceptor = nullptr;
  const AstNode *_node = nullptr;
  services::DiagnosticSeverity _severity =
      services::DiagnosticSeverity::Error;
  std::string _message;
  std::optional<services::DiagnosticCode> _code;
  std::optional<std::string> _codeDescription;
  std::vector<services::DiagnosticTag> _tags;
  std::vector<services::DiagnosticRelatedInformation> _relatedInformation;
  std::optional<services::JsonValue> _data;
  TextOffset _begin = 0;
  TextOffset _end = 0;
  bool _pending = true;
};

template <typename Node>
class ValidationDiagnosticBuilder final
    : public ValidationDiagnosticBuilderBase<ValidationDiagnosticBuilder<Node>> {
public:
  using ValidationDiagnosticBuilderBase<
      ValidationDiagnosticBuilder<Node>>::ValidationDiagnosticBuilderBase;

  template <auto Feature>
    requires pegium::detail::AstNodeFeature<Node, Feature>
  ValidationDiagnosticBuilder &property() noexcept {
    if (const auto *typedNode = static_cast<const Node *>(this->node());
        typedNode != nullptr) {
      this->setRange(range_for_feature<Feature>(*typedNode));
    }
    return *this;
  }

  template <auto Feature>
    requires pegium::detail::VectorAstNodeFeature<Node, Feature>
  ValidationDiagnosticBuilder &property(std::size_t index) noexcept {
    if (const auto *typedNode = static_cast<const Node *>(this->node());
        typedNode != nullptr) {
      this->setRange(range_for_feature<Feature>(*typedNode, index));
    }
    return *this;
  }
};

class ValidationAcceptor {
public:
  using Callback = std::function<void(services::Diagnostic)>;

  ValidationAcceptor() = default;

  template <typename CallbackType>
    requires std::constructible_from<Callback, CallbackType>
  ValidationAcceptor(CallbackType &&callback)
      : _callback(std::forward<CallbackType>(callback)) {}

  void operator()(services::Diagnostic diagnostic) const {
    if (_callback) {
      _callback(std::move(diagnostic));
    }
  }


  template <typename Node>
    requires std::derived_from<std::remove_cvref_t<Node>, AstNode>
  ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>
  error(const Node &node, std::string_view message) const noexcept {
    return ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>(
        *this, services::DiagnosticSeverity::Error, message, node);
  }

  template <typename Node>
    requires std::derived_from<std::remove_cvref_t<Node>, AstNode>
  ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>
  warning(const Node &node, std::string_view message) const noexcept {
    return ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>(
        *this, services::DiagnosticSeverity::Warning, message, node);
  }

  template <typename Node>
    requires std::derived_from<std::remove_cvref_t<Node>, AstNode>
  ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>
  info(const Node &node, std::string_view message) const noexcept {
    return ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>(
        *this, services::DiagnosticSeverity::Information, message, node);
  }

  template <typename Node>
    requires std::derived_from<std::remove_cvref_t<Node>, AstNode>
  ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>
  hint(const Node &node, std::string_view message) const noexcept {
    return ValidationDiagnosticBuilder<std::remove_cvref_t<Node>>(
        *this, services::DiagnosticSeverity::Hint, message, node);
  }

private:
  Callback _callback;
};

template <typename Derived>
inline ValidationDiagnosticBuilderBase<Derived>::ValidationDiagnosticBuilderBase(
    const ValidationAcceptor &acceptor, services::DiagnosticSeverity severity,
    std::string_view message, const AstNode &node) noexcept
    : _acceptor(&acceptor), _node(&node), _severity(severity), _message(message) {
  std::tie(_begin, _end) = range_of(node);
}

template <typename Derived>
inline void ValidationDiagnosticBuilderBase<Derived>::emit() {
  if (!_pending || _acceptor == nullptr) {
    return;
  }

  services::Diagnostic diagnostic;
  diagnostic.severity = _severity;
  diagnostic.message = _message;
  diagnostic.code = _code;
  diagnostic.codeDescription = _codeDescription;
  diagnostic.tags = _tags;
  diagnostic.relatedInformation = _relatedInformation;
  diagnostic.data = _data;
  diagnostic.begin = _begin;
  diagnostic.end = _end >= _begin ? _end : _begin;
  _acceptor->operator()(std::move(diagnostic));
  _pending = false;
}

} // namespace pegium::validation
