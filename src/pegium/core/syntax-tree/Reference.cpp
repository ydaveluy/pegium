#include <pegium/core/syntax-tree/Reference.hpp>

#include <cassert>
#include <string>
#include <typeindex>
#include <utility>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/parser/Introspection.hpp>

namespace pegium {

std::type_index AbstractReference::getReferenceType() const noexcept {
  return getAssignment().getType();
}

std::string_view AbstractReference::getFeature() const noexcept {
  return getAssignment().getFeature();
}

std::string AbstractReference::getErrorMessage() const {
  using enum ReferenceState;
  switch (_state.load(std::memory_order_acquire)) {
  case ErrorNoLinker:
    return "No linker is available for this reference.";
  case ErrorNotFound: {
    std::string message = "Could not resolve reference";
    if (const auto type = getReferenceType();
        type != std::type_index(typeid(void))) {
      auto typeName = parser::detail::runtime_type_name(type);
      assert(!typeName.empty());
      message += " to " + std::move(typeName);
    }
    message += " named '" + _refText + "'.";
    return message;
  }
  case ErrorCycle: {
    std::string feature(getFeature());
    if (feature.empty()) {
      feature = "<unknown>";
    }
    return "Cyclic reference resolution detected for feature '" + feature +
           "' (symbol '" + _refText + "').";
  }
  case ErrorException:
    return "An error occurred while resolving reference to '" + _refText + "'.";
  case Unresolved:
  case Resolving:
  case Resolved:
    break;
  }
  return {};
}

} // namespace pegium
