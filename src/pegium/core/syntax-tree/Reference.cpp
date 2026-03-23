#include <pegium/core/syntax-tree/Reference.hpp>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/references/Linker.hpp>

namespace pegium {

std::type_index AbstractReference::getReferenceType() const noexcept {
  return getAssignment().getType();
}

std::string_view AbstractReference::getFeature() const noexcept {
  return getAssignment().getFeature();
}

} // namespace pegium
