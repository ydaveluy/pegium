#pragma once

#if defined(PEGIUM_ENABLE_RECOVERY_TRACE)
#include <iostream>
#include <utility>
#endif

namespace pegium::parser::detail {

#if defined(PEGIUM_ENABLE_RECOVERY_TRACE)
template <typename... Args>
inline void recoveryTrace(Args &&...args) {
  ((std::cerr << std::forward<Args>(args)), ...);
  std::cerr << '\n';
}
#endif

} // namespace pegium::parser::detail

#if defined(PEGIUM_ENABLE_RECOVERY_TRACE)
#define PEGIUM_RECOVERY_TRACE(...)                                             \
  do {                                                                         \
    ::pegium::parser::detail::recoveryTrace(__VA_ARGS__);                     \
  } while (false)
#else
#define PEGIUM_RECOVERY_TRACE(...)                                             \
  do {                                                                         \
  } while (false)
#endif

