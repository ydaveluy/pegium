
#include <type_traits>
#include <utility>

struct A {

    double x = 0;
    double y = 0;
    double z = 0;
};
struct B {
  constexpr B(int) {}
  B(const B &) = delete;
  B(B &&) = delete;
  B &operator=(const B &) = delete;
  B operator=(B &&) = delete;
};
static constexpr A a;
static constexpr B b{0};

template <typename T>
using wrap =
    std::conditional_t<std::is_copy_constructible_v<std::remove_cvref_t<T>>,
                       std::remove_cvref_t<T>, T>;

template <typename T> wrap<T> test(T &&element) {
  return std::forward<T>(element);
}

template <typename Element> struct NotPredicate {
  explicit constexpr NotPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}

private:
  Element _element;
};

template <typename T>
[[nodiscard ]]
constexpr wrap<T>
forward(typename std::remove_reference<T>::type &element) noexcept {
  if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<T>> &&
                std::is_lvalue_reference_v<T>)
    return std::remove_cvref_t<T>{element};
  else
    return element;
}

template <typename Element> constexpr auto operator!(Element &&element) {
  /*if constexpr (std::is_copy_constructible_v<std::remove_cvref_t<Element>> &&
                std::is_lvalue_reference_v<Element>)
    return NotPredicate<wrap<Element>>{std::remove_cvref_t<Element>{element}};
  else
    return NotPredicate<wrap<Element>>{std::forward<Element>(element)};*/
  return NotPredicate<wrap<Element>>{forward<Element>(element)};
}

static constexpr auto d = !b;
static constexpr auto c = !a;
static constexpr auto e = !A{};

static_assert(sizeof(d)==8);
static_assert(sizeof(c)==1);
static_assert(sizeof(e)==1);
//static constexpr auto f = !B{0};

static_assert(std::is_same_v<A, decltype(test(a))>);
static_assert(std::is_same_v<const B &, decltype(test(b))>);
static_assert(std::is_same_v<A, decltype(test(A{}))>);
static_assert(std::is_same_v<B, decltype(test(B{0}))>);
