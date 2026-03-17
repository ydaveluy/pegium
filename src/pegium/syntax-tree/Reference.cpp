#include <pegium/syntax-tree/Reference.hpp>

#include <pegium/references/Linker.hpp>
namespace pegium {

void AbstractReference::ensureResolved() const {
  auto state = _state.load(std::memory_order_acquire);
  if (state == ReferenceState::Resolved || state == ReferenceState::Error) {
    return;
  }

  std::scoped_lock lock(_mutex);
  state = _state.load(std::memory_order_relaxed);
  if (state == ReferenceState::Resolved || state == ReferenceState::Error) {
    return;
  }
  if (state == ReferenceState::Resolving) {
    _targets.clear();
    _errorMessage = "Cyclic reference resolution detected.";
    _state.store(ReferenceState::Error, std::memory_order_release);
    return;
  }

  if (_linker == nullptr) {
    _targets.clear();
    _errorMessage = "No linker is available for this reference.";
    _state.store(ReferenceState::Error, std::memory_order_release);
    return;
  }

  _targets.clear();
  _errorMessage.clear();
  _state.store(ReferenceState::Resolving, std::memory_order_release);

  if (_isMulti) {
    setResolution(references::resolveAllReferences(*_linker, *this));
  } else {
    setResolution(references::resolveReference(*_linker, *this));
  }
}

} // namespace pegium
