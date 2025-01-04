#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <pegium/Parser.hpp>
#include <pegium/syntax-tree.hpp>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace pegium {
using namespace grammar;

static_assert("test"_kw.parse_terminal("test123") == 4);
constexpr auto a = "a"_kw;
constexpr auto b = "b"_kw;
constexpr auto c = "c"_kw;

static_assert(a.parse_terminal("a") == 1);
static_assert(a.i().parse_terminal("A") == 1);

static_assert((a + b).parse_terminal("abaa") == 2);

static_assert((a | b).parse_terminal("ab") == 1);
static_assert((a | b).parse_terminal("ba") == 1);
static_assert((a | b).parse_terminal("c") == PARSE_ERROR);
static_assert(((a | b) | (c | d)).parse_terminal("b") == 1);
static_assert(((a | b) | (c | d)).parse_terminal("c") == 1);
static_assert((a | (c | b)).parse_terminal("c") == 1);
static_assert(((a | b) | c).parse_terminal("c") == 1);

static_assert(((a + a) + a).parse_terminal("aaaa") == 3);
static_assert(((a + a) + (a + a)).parse_terminal("aaaa") == 4);
static_assert((a + (a + a)).parse_terminal("aaaa") == 3);

static_assert(opt(a).parse_terminal("a") == 1);
static_assert(opt(a).parse_terminal("") == 0);
static_assert(opt(a).parse_terminal("b") == 0);

static_assert((at_least_one(a)).parse_terminal("a") == 1);
static_assert((at_least_one(a)).parse_terminal("aaa") == 3);
static_assert((at_least_one(a)).parse_terminal("") == PARSE_ERROR);
static_assert((at_least_one(a)).parse_terminal("b") == PARSE_ERROR);

static_assert((many(a)).parse_terminal("a") == 1);
static_assert((many(a)).parse_terminal("aaa") == 3);
static_assert((many(a)).parse_terminal("") == 0);
static_assert((many(a)).parse_terminal("b") == 0);

static_assert("a-z"_cr.i().parse_terminal("B") == 1);
static_assert(("a-z"_cr | "A-Z"_cr).i().parse_terminal("+") == PARSE_ERROR);



} // namespace pegium