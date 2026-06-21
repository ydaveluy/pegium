#include <gtest/gtest.h>

#include <arithmetics/lsp/Module.hpp>
#include <arithmetics/core/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace arithmetics::tests {
namespace {

TEST(ArithmeticsLanguageTest, GarbageBeforeBrokenCallKeepsRecoveryLocal) {
  struct Case {
    const char *name;
    std::string text;
    const char *uri;
    std::size_t minStatements;
  };

  static const Case kCases[] = {
      {"GarbageBlockBeforeBrokenCallKeepsFollowingCallRecoveryLocal",
       "Module basicMath\n"
       "\n"
       "def a: 5;\n"
       "def b: 3;\n"
       "\n"
       "def c: a + b; // 8\n"
       "def d: (a ^ b); // 164\n"
       "\n"
       "def root(x, y):\n"
       "    x^(1/y);\n"
       "\n"
       "def sqrt(x):\n"
       "    root(x, 2);\n"
       "\n"
       "2 * c; // 16\n"
       "b % 2; // 1\n"
       "\n"
       "// This language is case-insensitive regarding symbol names\n"
       "Root(D, 3); // 32\n"
       "Root(64, 3); // 4\n"
       "\n"
       "xxxxx\n"
       "xxxxxxxxxxxx;\n"
       "\n"
       "Root(64 + 5 3/0+5-3); // 4\n",
       "garbage-block-before-broken-call-keeps-following-call-local.calc", 12u},
      {"ShorterGarbageBlockBeforeBrokenCallKeepsFollowingCallRecoveryLocal",
       "Module basicMath\n"
       "\n"
       "def a: 5;\n"
       "def b: 3;\n"
       "\n"
       "def c: a + b; // 8\n"
       "def d: (a ^ b); // 164\n"
       "\n"
       "def root(x, y):\n"
       "    x^(1/y);\n"
       "\n"
       "def sqrt(x):\n"
       "    root(x, 2);\n"
       "\n"
       "2 * c; // 16\n"
       "b % 2; // 1\n"
       "\n"
       "// This language is case-insensitive regarding symbol names\n"
       "Root(D, 3); // 32\n"
       "Root(64, 3); // 4\n"
       "\n"
       "xxxxx\n"
       "xxxxxxxxxxx;\n"
       "\n"
       "Root(64 + 5 3/0+5-3); // 4\n",
       "shorter-garbage-block-before-broken-call-keeps-following-call-local."
       "calc",
       12u},
      {"SplitGarbageBlockBeforeBrokenCallKeepsFollowingCallRecoveryLocal",
       "Module basicMath\n"
       "\n"
       "def a: 5;\n"
       "def b: 3;\n"
       "\n"
       "def c: a + b; // 8\n"
       "def d: (a ^ b); // 164\n"
       "\n"
       "def root(x, y):\n"
       "    x^(1/y);\n"
       "\n"
       "def sqrt(x):\n"
       "    root(x, 2);\n"
       "\n"
       "2 * c; // 16\n"
       "b % 2; // 1\n"
       "\n"
       "// This language is case-insensitive regarding symbol names\n"
       "Root(D, 3); // 32\n"
       "Root(64, 3); // 4\n"
       "xx\n"
       "xxxxxxxxxxxx;\n"
       "xx\n"
       "\n"
       "Root(64 + 5 3/0+5-3); // 4\n",
       "split-garbage-block-before-broken-call-keeps-following-call-local.calc",
       12u},
      {"SpacedGarbageRunBeforeComplexBrokenCallKeepsFollowingCallRecoveryLocal",
       "Module basicMath\n"
       "\n"
       "def a: 5;\n"
       "def b: 3;\n"
       "\n"
       "def c: a + b; // 8\n"
       "def d: (a ^ b); // 164\n"
       "\n"
       "def root(x, y):\n"
       "    x^(1/y);\n"
       "\n"
       "def sqrt(x):\n"
       "    root(x, 2);\n"
       "\n"
       "2 * c; // 16\n"
       "b % 2; // 1\n"
       "\n"
       "// This language is case-insensitive regarding symbol names\n"
       "Root(D, 3); // 32\n"
       "Root(64, 3); // 4\n"
       "xx\n"
       "xxxxxxxxxxxx;\n"
       "x    x x x x x x x\n"
       "\n"
       "Root(64 + 5 3/0+5-3); // 4\n",
       "spaced-garbage-run-before-complex-broken-call-keeps-following-call-"
       "local.calc",
       13u},
      {"ShorterSpacedGarbageRunBeforeComplexBrokenCallKeepsFollowingCallRecovery"
       "Local",
       "Module basicMath\n"
       "\n"
       "def a: 5;\n"
       "def b: 3;\n"
       "\n"
       "def c: a + b; // 8\n"
       "def d: (a ^ b); // 164\n"
       "\n"
       "def root(x, y):\n"
       "    x^(1/y);\n"
       "\n"
       "def sqrt(x):\n"
       "    root(x, 2);\n"
       "\n"
       "2 * c; // 16\n"
       "b % 2; // 1\n"
       "\n"
       "// This language is case-insensitive regarding symbol names\n"
       "Root(D, 3); // 32\n"
       "Root(64, 3); // 4\n"
       "xx\n"
       "xxxxxxxxxxxx;\n"
       "x    x x x x x x\n"
       "\n"
       "Root(64 + 5 3/0+5-3); // 4\n",
       "shorter-spaced-garbage-run-before-complex-broken-call-keeps-following-"
       "call-local.calc",
       13u},
  };

  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);

    parser::ArithmeticParser parser;
    auto document = pegium::test::parse_document(
        parser, c.text, pegium::test::make_file_uri(c.uri), "arithmetics");

    const auto &parsed = document->parseResult;
    const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
    ASSERT_TRUE(parsed.value) << parseDump;
    EXPECT_TRUE(parsed.fullMatch) << parseDump;
    EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

    auto *module = dynamic_cast<ast::Module *>(parsed.value);
    ASSERT_NE(module, nullptr) << parseDump;
    ASSERT_GE(module->statements.size(), c.minStatements)
        << parseDump << " :: " << summarize_module_statement_shapes(*module);

    auto *lastEvaluation =
        dynamic_cast<ast::Evaluation *>(module->statements.back());
    ASSERT_NE(lastEvaluation, nullptr)
        << parseDump << " :: " << summarize_module_statement_shapes(*module);
    auto *lastCall =
        dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression);
    ASSERT_NE(lastCall, nullptr)
        << parseDump << " :: " << summarize_module_statement_shapes(*module);
    EXPECT_EQ(lastCall->func.getRefText(), "root")
        << parseDump << " :: " << summarize_module_statement_shapes(*module);
    EXPECT_FALSE(lastCall->args.empty())
        << parseDump << " :: " << summarize_module_statement_shapes(*module);
  }
}

} // namespace
} // namespace arithmetics::tests
