#pragma once

/// Core parser expression concepts, wrappers, and dispatch helpers.

#include <cassert>
#include <concepts>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <string>
#include <type_traits>
#include <utility>

namespace pegium::parser {

struct ParseContext;
struct TrackedParseContext;
struct RecoveryContext;
struct ExpectContext;
struct AstReflectionInitContext;

template <typename Expr>
  requires std::derived_from<std::remove_cvref_t<Expr>, grammar::AbstractElement>
void init(const Expr &expression, AstReflectionInitContext &ctx);

namespace detail {

struct InitAccess;

template <typename T> struct IsTerminalAtom : std::false_type {};

template <typename T>
inline constexpr bool IsTerminalAtom_v =
    IsTerminalAtom<std::remove_cvref_t<T>>::value;

} // namespace detail

// An expression is failure-safe when `parse(expr, ctx) == false` guarantees that the
// caller does not need to rewind the CST/cursor state to discard partial work.

template <typename E>
concept Expression =
    std::derived_from<std::remove_cvref_t<E>, grammar::AbstractElement> &&
    requires {
      { std::remove_cvref_t<E>::nullable } -> std::convertible_to<bool>;
      { std::remove_cvref_t<E>::isFailureSafe } -> std::convertible_to<bool>;
    } &&
    requires(const std::remove_cvref_t<E> &expression, ParseContext &ctx,
             RecoveryContext &recoveryCtx,
             ExpectContext &expectCtx) {
      { probe(expression, ctx) } -> std::same_as<bool>;
      { parse(expression, ctx) } -> std::same_as<bool>;
      { parse(expression, recoveryCtx) } -> std::same_as<bool>;
      { parse(expression, expectCtx) } -> std::same_as<bool>;
    };

template <typename E>
concept TerminalCapableExpression =
    Expression<E> &&
    requires(const std::remove_cvref_t<E> &expression, const char *begin) {
      { expression.terminal(begin) } noexcept -> std::same_as<const char *>;
    };

template <typename E>
concept RecoveryProbeCapableExpression =
    Expression<E> &&
    requires(const std::remove_cvref_t<E> &expression, RecoveryContext &ctx) {
      { expression.probeRecoverable(ctx) } -> std::same_as<bool>;
    };

template <Expression E>
using ExpressionHolder = std::conditional_t<std::is_lvalue_reference_v<E>, E,
                                            std::remove_cvref_t<E>>;

template <typename E>
concept NonNullableExpression =
    Expression<E> && !std::remove_cvref_t<E>::nullable;

template <typename E>
concept NonNullableTerminalCapableExpression =
    TerminalCapableExpression<E> && !std::remove_cvref_t<E>::nullable;

template <typename E>
concept TerminalAtom =
    TerminalCapableExpression<E> && detail::IsTerminalAtom_v<E>;

struct Wrapper {
  struct Ops {
    using ParseFn = bool (*)(const void *, ParseContext &);
    using TrackedParseFn = bool (*)(const void *, TrackedParseContext &);
    using RecoveryParseFn = bool (*)(const void *, RecoveryContext &);
    using ExpectParseFn = bool (*)(const void *, ExpectContext &);
    using RecoveryProbeFn = bool (*)(const void *, RecoveryContext &);
    using TerminalFn = const char *(*)(const void *, const char *) noexcept;
    using ElemFn = const grammar::AbstractElement *(*)(const void *) noexcept;
    using InitFn = void (*)(const void *, AstReflectionInitContext &);
    using DeleteFn = void (*)(void *) noexcept;
    using CloneFn = void *(*)(const void *);

    ParseFn parse = nullptr;
    TrackedParseFn parseTracked = nullptr;
    RecoveryParseFn parseRecover = nullptr;
    ExpectParseFn parseExpect = nullptr;
    RecoveryProbeFn probeRecoverable = nullptr;
    TerminalFn terminal = nullptr;
    ElemFn elem = nullptr;
    InitFn init = nullptr;
    DeleteFn destroy = nullptr;
    CloneFn clone = nullptr;
  };

  Wrapper() = default;

  Wrapper(const Wrapper &other) : _ops(other._ops) {
    if (other._obj)
      _obj = _ops.clone(other._obj);
  }

  Wrapper(Wrapper &&other) noexcept { move_from(std::move(other)); }

  Wrapper &operator=(const Wrapper &other) {
    if (this == &other)
      return *this;

    reset();
    _ops = other._ops;
    if (other._obj)
      _obj = _ops.clone(other._obj);
    return *this;
  }

  Wrapper &operator=(Wrapper &&other) noexcept {
    if (this == &other)
      return *this;
    reset();
    move_from(std::move(other));
    return *this;
  }

  ~Wrapper() { reset(); }

  template <Expression Element> void set(Element &&element) {
    using W = Model<Element>;

    reset();
    _ops = W::ops;
    _obj = new W(std::forward<Element>(element));
  }

  bool has_value() const noexcept { return _obj != nullptr; }

  bool has_terminal() const noexcept {
    return _obj != nullptr && _ops.terminal != nullptr;
  }

  bool has_recovery_probe() const noexcept {
    return _obj != nullptr && _ops.probeRecoverable != nullptr;
  }

  const char *try_terminal(const char *begin) const noexcept {
    return has_terminal() ? _ops.terminal(_obj, begin) : nullptr;
  }

  const char *try_terminal(const std::string &text) const noexcept {
    return try_terminal(text.c_str());
  }

  bool probe_recoverable(RecoveryContext &ctx) const {
    return has_recovery_probe() ? _ops.probeRecoverable(_obj, ctx) : false;
  }

  const grammar::AbstractElement *element() const noexcept {
    assert(_obj && _ops.elem && "Missing element wrapper!");
    return _ops.elem(_obj);
  }

  void init(AstReflectionInitContext &ctx) const {
    assert(_obj && _ops.init && "Missing init wrapper!");
    _ops.init(_obj, ctx);
  }

  void reset() noexcept {
    if (!_obj)
      return;
    _ops.destroy(_obj);
    _obj = nullptr;
    _ops = {};
  }

private:
  friend struct detail::ParseAccess;

  void move_from(Wrapper &&other) noexcept {
    _obj = std::exchange(other._obj, nullptr);
    _ops = std::exchange(other._ops, {});
  }

  template <ParseModeContext Context>
  bool parse_impl(Context &ctx) const {
    if constexpr (std::same_as<std::remove_cvref_t<Context>, ParseContext>) {
      assert(_obj && _ops.parse && "Missing element wrapper!");
      return _ops.parse(_obj, ctx);
    } else if constexpr (std::same_as<std::remove_cvref_t<Context>,
                                      TrackedParseContext>) {
      assert(_obj && _ops.parseTracked && "Missing tracked element wrapper!");
      return _ops.parseTracked(_obj, ctx);
    } else if constexpr (RecoveryParseModeContext<Context>) {
      assert(_obj && _ops.parseRecover && "Missing element wrapper!");
      return _ops.parseRecover(_obj, ctx);
    } else {
      assert(_obj && _ops.parseExpect && "Missing element wrapper!");
      return _ops.parseExpect(_obj, ctx);
    }
  }

  template <class T> struct Model {

    explicit Model(T &&v)
      requires(!std::is_lvalue_reference_v<T>)
        : value(std::move(v)) {}

    explicit Model(T v)
      requires(std::is_lvalue_reference_v<T>)
        : value(v) {}

    static bool parse(const void *self, ParseContext &ctx) {
      return parser::parse(static_cast<const Model *>(self)->value, ctx);
    }
    static bool parseTracked(const void *self, TrackedParseContext &ctx) {
      return parser::parse(static_cast<const Model *>(self)->value, ctx);
    }
    static bool parseRecover(const void *self, RecoveryContext &ctx) {
      return parser::parse(static_cast<const Model *>(self)->value, ctx);
    }
    static bool parseExpect(const void *self, ExpectContext &ctx) {
      return parser::parse(static_cast<const Model *>(self)->value, ctx);
    }
    static bool probeRecoverable(const void *self, RecoveryContext &ctx)
      requires RecoveryProbeCapableExpression<T>
    {
      return static_cast<const Model *>(self)->value.probeRecoverable(ctx);
    }
    static const char *terminal(const void *self, const char *b) noexcept
      requires TerminalCapableExpression<T>
    {
      return static_cast<const Model *>(self)->value.terminal(b);
    }
    static const grammar::AbstractElement *elem(const void *self) noexcept {
      return std::addressof(static_cast<const Model *>(self)->value);
    }
    static void init(const void *self, AstReflectionInitContext &ctx) {
      parser::init(static_cast<const Model *>(self)->value, ctx);
    }
    static void del(void *self) noexcept { delete static_cast<Model *>(self); }
    static void *clone(const void *self) {
      return new Model(*static_cast<const Model *>(self));
    }

    static constexpr Ops make_ops() noexcept {
      Ops ops{
          .parse = &Model::parse,
          .parseTracked = &Model::parseTracked,
          .parseRecover = &Model::parseRecover,
          .parseExpect = &Model::parseExpect,
          .elem = &Model::elem,
          .init = &Model::init,
          .destroy = &Model::del,
          .clone = &Model::clone,
      };
      if constexpr (RecoveryProbeCapableExpression<T>) {
        ops.probeRecoverable = &Model::probeRecoverable;
      }
      if constexpr (TerminalCapableExpression<T>) {
        ops.terminal = &Model::terminal;
      }
      return ops;
    }

    inline static constexpr Ops ops = make_ops();

  private:
    ExpressionHolder<T> value;
  };

  void *_obj = nullptr; // heap-only
  Ops _ops{};
};
} // namespace pegium::parser
