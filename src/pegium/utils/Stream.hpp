#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <type_traits>
#include <vector>
#include <utility>
#include <variant>

namespace pegium::utils {

template <class T>
class stream : public std::ranges::view_interface<stream<T>> {
public:
  stream() = default;

  // Small type-erasure: we store a polymorphic generator that yields `const T*`
  struct generator {
    virtual ~generator() noexcept = default;
    virtual const T* next() noexcept = 0; // nullptr when finished
  };

  explicit stream(std::unique_ptr<generator> g) noexcept : gen_(std::move(g)) {}

  stream(stream&&) noexcept = default;
  stream& operator=(stream&&) noexcept = default;

  stream(const stream&) = delete;
  stream& operator=(const stream&) = delete;

  class iterator {
  public:
    using iterator_concept = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    explicit iterator(generator* g) noexcept : g_(g) { ++(*this); } // prime

    const T& operator*() const noexcept { return *cur_; }
    const T* operator->() const noexcept { return cur_; }

    iterator& operator++() noexcept {
      if (!g_) {
        cur_ = nullptr;
        return *this;
      }
      cur_ = g_->next();
      if (!cur_) g_ = nullptr;
      return *this;
    }
    void operator++(int) noexcept { ++(*this); }

    friend bool operator==(const iterator& it, std::default_sentinel_t) noexcept {
      return it.g_ == nullptr;
    }

  private:
    generator* g_ = nullptr;
    const T* cur_ = nullptr;
  };

  iterator begin() noexcept { return iterator{gen_.get()}; }
  std::default_sentinel_t end() const noexcept { return {}; }

  explicit operator bool() const noexcept { return static_cast<bool>(gen_); }

private:
  std::unique_ptr<generator> gen_;
};

template <class T, std::ranges::input_range R>
requires std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, T>
class range_generator final : public stream<T>::generator {
public:
  explicit range_generator(R r) : r_(std::move(r)) {
    it_ = std::ranges::begin(r_);
    end_ = std::ranges::end(r_);
  }

  const T* next() noexcept override {
    if (done_) return nullptr;
    if (it_ == end_) {
      done_ = true;
      return nullptr;
    }
    const T* p = std::addressof(*it_);
    ++it_;
    return p;
  }

private:
  R r_;
  std::ranges::iterator_t<R> it_{};
  std::ranges::sentinel_t<R> end_{};
  bool done_ = false;
};

template <class T, std::ranges::input_range R>
requires std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, T>
stream<T> make_stream(R&& r) {
  using V = std::views::all_t<R>;
  return stream<T>(std::make_unique<range_generator<T, V>>(std::views::all(std::forward<R>(r))));
}

// Déduction automatique de T
template <std::ranges::input_range R>
stream<std::remove_cvref_t<std::ranges::range_reference_t<R>>> make_stream(R&& r) {
  using T = std::remove_cvref_t<std::ranges::range_reference_t<R>>;
  return make_stream<T>(std::forward<R>(r));
}

//------------------------------------------------------------------------------
// 3) Singleton stream
//------------------------------------------------------------------------------
template <class T>
stream<T> single_stream(T const& value) {
  // views::single produit T (copie) si on lui donne une valeur.
  // Ici on veut une référence stable: on wrappe via transform sur un pointeur.
  struct gen final : stream<T>::generator {
    explicit gen(const T* p) : p_(p) {}
    const T* next() noexcept override {
      if (done_) return nullptr;
      done_ = true;
      return p_;
    }
    const T* p_;
    bool done_ = false;
  };
  return stream<T>(std::make_unique<gen>(std::addressof(value)));
}


template <class T, class... Ss>
requires (std::same_as<std::remove_cvref_t<Ss>, utils::stream<T>> && ...)
[[nodiscard]] utils::stream<T> join(Ss&&... ss) {
  constexpr std::size_t N = sizeof...(Ss);

  if constexpr (N == 0) {
    return utils::make_stream<T>(std::views::empty<T>);
  } else {
    auto streams = std::array<utils::stream<T>, N>{ std::forward<Ss>(ss)... };
    auto joined  = std::views::all(std::move(streams)) | std::views::join;
    return utils::make_stream<T>(std::move(joined));
  }
}

template <class T>
[[nodiscard]] std::vector<T> collect(utils::stream<T> s) {
  std::vector<T> out;
  for (const auto &value : s) {
    out.push_back(value);
  }
  return out;
}

template <class T, class... Ss>
requires (std::same_as<std::remove_cvref_t<Ss>, utils::stream<T>> && ...)
[[nodiscard]] utils::stream<T> concat(Ss&&... ss) {
  return join<T>(std::forward<Ss>(ss)...);
}

template <class T, class Pred>
requires std::invocable<Pred, const T &> &&
         std::convertible_to<std::invoke_result_t<Pred, const T &>, bool>
[[nodiscard]] utils::stream<T> filter(utils::stream<T> s, Pred pred) {
  std::vector<T> out;
  for (const auto &value : s) {
    if (std::invoke(pred, value)) {
      out.push_back(value);
    }
  }
  return utils::make_stream<T>(std::move(out));
}

template <class T, class Fn>
requires std::invocable<Fn, const T &>
[[nodiscard]] auto map(utils::stream<T> s, Fn fn)
    -> utils::stream<std::remove_cvref_t<std::invoke_result_t<Fn, const T &>>> {
  using U = std::remove_cvref_t<std::invoke_result_t<Fn, const T &>>;
  std::vector<U> out;
  for (const auto &value : s) {
    out.push_back(std::invoke(fn, value));
  }
  return utils::make_stream<U>(std::move(out));
}

template <class T>
requires std::equality_comparable<T>
[[nodiscard]] utils::stream<T> distinct(utils::stream<T> s) {
  std::vector<T> out;
  for (const auto &value : s) {
    if (std::ranges::find(out, value) == out.end()) {
      out.push_back(value);
    }
  }
  return utils::make_stream<T>(std::move(out));
}

template <class T, class KeyFn>
requires std::invocable<KeyFn, const T &> &&
         std::equality_comparable<std::remove_cvref_t<
             std::invoke_result_t<KeyFn, const T &>>>
[[nodiscard]] utils::stream<T> distinct(utils::stream<T> s, KeyFn keyFn) {
  using Key = std::remove_cvref_t<std::invoke_result_t<KeyFn, const T &>>;
  std::vector<Key> seenKeys;
  std::vector<T> out;

  for (const auto &value : s) {
    const auto key = std::invoke(keyFn, value);
    if (std::ranges::find(seenKeys, key) != seenKeys.end()) {
      continue;
    }
    seenKeys.push_back(key);
    out.push_back(value);
  }

  return utils::make_stream<T>(std::move(out));
}

template <class T, class Acc, class Fn>
requires std::invocable<Fn, Acc, const T &>
[[nodiscard]] auto reduce(utils::stream<T> s, Acc init, Fn fn)
    -> std::remove_cvref_t<std::invoke_result_t<Fn, Acc, const T &>> {
  using Result = std::remove_cvref_t<std::invoke_result_t<Fn, Acc, const T &>>;
  Result acc = std::move(init);
  for (const auto &value : s) {
    acc = std::invoke(fn, std::move(acc), value);
  }
  return acc;
}
} // namespace pegium::utils
