#include "xsmp.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/parser/Literal.hpp>
#include <pegium/parser/Group.hpp>

using namespace pegium::parser;
constexpr auto l = "aaa"_kw + "bbbb"_kw;