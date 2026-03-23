#pragma once

#include <cassert>
#include <concepts>
#include <functional>
#include <type_traits>

namespace pegium::utils {

template <typename> class function_ref;

/// Non-owning callable view for lightweight callback parameters.
template <typename R, typename... Args>
class function_ref<R(Args...)> {
public:
  function_ref() noexcept = default;
  explicit function_ref(std::nullptr_t) noexcept {}

  template <typename F>
    requires (!std::same_as<std::remove_cvref_t<F>, function_ref> &&
              std::is_lvalue_reference_v<F&&> &&
              std::is_invocable_r_v<R, const F&, Args&&...>)
  explicit function_ref(F&& callable) noexcept
      : _callable(std::addressof(callable)),
        _callback([](const void* callablePtr, Args&&... args) -> R {
          return std::invoke(
              *static_cast<const std::remove_reference_t<F>*>(callablePtr),
              std::forward<Args>(args)...);
        }) {}

  [[nodiscard]] explicit operator bool() const noexcept {
    return _callback != nullptr;
  }

  R operator()(Args&&... args) const {
    assert(_callback != nullptr);
    return _callback(_callable, std::forward<Args>(args)...);
  }

private:
  const void* _callable = nullptr;
  R (*_callback)(const void*, Args&&...) = nullptr;
};
} // namespace pegium::utils
