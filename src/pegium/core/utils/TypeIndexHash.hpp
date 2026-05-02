#pragma once

#include <cstddef>
#include <cstdint>
#include <typeindex>
#include <typeinfo>

namespace pegium::utils {

/// Fast hash for `std::type_index`.
///
/// `std::type_index::hash_code()` falls back to hashing the mangled type
/// name (`std::_Hash_bytes`) on libstdc++ unless `__GXX_TYPEINFO_EQUALITY_INLINE`
/// is defined — which it isn't on most Linux toolchains. Profiling shows this
/// hash dominating hot paths that look up types repeatedly (the scope provider
/// runs `accepts_bucket` once per bucket per reference resolution and
/// `_supertypesByType` is the inner-most data structure).
///
/// Within a single process, `type_info` objects are unique per type, so we can
/// hash the address of the type's name string (which `std::type_index` exposes
/// via `name()`) and skip the per-character work entirely.
struct FastTypeIndexHash {
  [[nodiscard]] std::size_t
  operator()(const std::type_index &index) const noexcept {
    // `name()` returns a pointer to the same C string for the same type within
    // a single binary; using its address keeps the hash O(1) and stable.
    const auto bits = reinterpret_cast<std::uintptr_t>(index.name());
    return static_cast<std::size_t>(bits);
  }
};

/// Pointer-based equality for `std::type_index`.
///
/// `std::equal_to<std::type_index>` defers to `type_index::operator==`, which
/// `strcmp`s the mangled type name on libstdc++ for cross-DSO safety. Within a
/// single binary `type_info` objects are unique per type, so comparing the
/// `name()` pointer is both correct and one instruction.
struct FastTypeIndexEqual {
  [[nodiscard]] bool operator()(const std::type_index &a,
                                const std::type_index &b) const noexcept {
    return a.name() == b.name();
  }
};

} // namespace pegium::utils
