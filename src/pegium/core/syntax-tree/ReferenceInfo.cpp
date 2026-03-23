#include <pegium/core/syntax-tree/ReferenceInfo.hpp>

#include <pegium/core/grammar/Assignment.hpp>

namespace pegium {

const grammar::Assignment &ReferenceInfo::getAssignment() const noexcept {
  return _assignment.get();
}

std::type_index ReferenceInfo::getReferenceType() const noexcept {
  return getAssignment().getType();
}

std::string_view ReferenceInfo::getFeature() const noexcept {
  return getAssignment().getFeature();
}

} // namespace pegium
