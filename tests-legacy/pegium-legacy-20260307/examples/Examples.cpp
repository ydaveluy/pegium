#include <gtest/gtest.h>

#include <arithmetics/services/Module.hpp>
#include <domainmodel/services/Module.hpp>
#include <requirements/services/Module.hpp>
#include <statemachine/services/Module.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include <pegium/lsp/DefaultLanguageServer.hpp>

namespace {

pegium::TextOffset find_offset(std::string_view text, std::string_view token) {
  const auto pos = text.find(token);
  EXPECT_NE(pos, std::string::npos);
  return pos == std::string::npos ? 0u : static_cast<pegium::TextOffset>(pos);
}

::lsp::Position position_from_offset(std::string_view text,
                                     pegium::TextOffset offset) {
  std::uint32_t line = 0;
  std::uint32_t character = 0;
  for (pegium::TextOffset index = 0; index < offset && index < text.size();
       ++index) {
    if (text[index] == '\n') {
      ++line;
      character = 0;
      continue;
    }
    ++character;
  }
  return {.line = line, .character = character};
}

bool has_message(std::span<const pegium::services::Diagnostic> diagnostics,
                 std::string_view needle) {
  return std::ranges::any_of(diagnostics, [needle](const auto &diagnostic) {
    return diagnostic.message.find(needle) != std::string::npos;
  });
}

const pegium::services::Diagnostic *
find_diagnostic(std::span<const pegium::services::Diagnostic> diagnostics,
                std::string_view needle) {
  const auto it = std::ranges::find_if(diagnostics, [needle](const auto &diagnostic) {
    return diagnostic.message.find(needle) != std::string::npos;
  });
  return it == diagnostics.end() ? nullptr : std::addressof(*it);
}

} // namespace

TEST(ExampleParityTest, ArithmeticsProvidesNormalizableDiagnosticAndQuickFix) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(arithmetics::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///arithmetics/example.calc";
  const std::string text = R"(module Demo
def value: 1 + 2;
value;
)";

  ASSERT_TRUE(server.didOpen(uri, "arithmetics", text));

  const auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(uri);
  const auto normalizable = std::ranges::find_if(
      diagnostics, [](const auto &diagnostic) {
        return diagnostic.code.has_value() &&
               *diagnostic.code == "arithmetics.expression-normalizable";
      });
  ASSERT_NE(normalizable, diagnostics.end());
  EXPECT_GT(normalizable->begin, 0u);
  EXPECT_GT(normalizable->end, normalizable->begin);

  const auto rangeBegin = find_offset(text, "1 + 2");
  const auto rangeEnd = static_cast<pegium::TextOffset>(rangeBegin + 5);
  ::lsp::CodeActionParams codeActionParams{};
  codeActionParams.textDocument.uri = ::lsp::Uri::parse(uri);
  codeActionParams.range = {
      .start = position_from_offset(text, rangeBegin),
      .end = position_from_offset(text, rangeEnd)};
  for (const auto &diagnostic : diagnostics) {
    ::lsp::Diagnostic lspDiagnostic{};
    lspDiagnostic.severity = diagnostic.severity;
    lspDiagnostic.message = diagnostic.message;
    if (!diagnostic.source.empty()) {
      lspDiagnostic.source = diagnostic.source;
    }
    lspDiagnostic.range = {
        .start = position_from_offset(text, diagnostic.begin),
        .end = position_from_offset(text, diagnostic.end)};
    if (diagnostic.code.has_value()) {
      lspDiagnostic.code = ::lsp::OneOf<int, ::lsp::String>(*diagnostic.code);
    }
    if (diagnostic.data.has_value()) {
      lspDiagnostic.data = *diagnostic.data;
    }
    codeActionParams.context.diagnostics.push_back(std::move(lspDiagnostic));
  }
  const auto actions = server.getCodeActions(codeActionParams);
  ASSERT_FALSE(actions.empty());
  EXPECT_EQ(actions.front().kind, "quickfix");
  EXPECT_TRUE(actions.front().isPreferred);
  ASSERT_TRUE(actions.front().edit.changes.contains(uri));
  ASSERT_EQ(actions.front().edit.changes.at(uri).size(), 1u);
  EXPECT_EQ(actions.front().edit.changes.at(uri).front().newText, "3.000000");

  ::lsp::CompletionParams completionParams{};
  completionParams.textDocument.uri = ::lsp::Uri::parse(uri);
  completionParams.position =
      position_from_offset(text, find_offset(text, "value;"));
  const auto completion = server.getCompletion(completionParams);
  EXPECT_TRUE(std::ranges::any_of(completion, [](const auto &item) {
    return item.label == "value";
  }));
}

TEST(ExampleParityTest, ArithmeticsDivisionByZeroDiagnosticHasExpectedRange) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(arithmetics::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///arithmetics/div-zero.calc";
  const std::string text = R"(module Demo
def value: 10 / 0;
value;
)";

  ASSERT_TRUE(server.didOpen(uri, "arithmetics", text));

  const auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(uri);
  const auto *diagnostic =
      find_diagnostic(diagnostics, "Division by zero is detected.");
  ASSERT_NE(diagnostic, nullptr);

  const auto expectedBegin = find_offset(text, "10 / 0");
  const auto expectedEnd =
      static_cast<pegium::TextOffset>(expectedBegin + std::string_view("10 / 0").size());

  EXPECT_EQ(diagnostic->begin, expectedBegin);
  EXPECT_EQ(diagnostic->end, expectedEnd);
}

TEST(ExampleParityTest,
     ArithmeticsDivisionByZeroRangeExcludesTrailingWhitespaceBeforePlus) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(arithmetics::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///arithmetics/div-zero-spaces.calc";
  const std::string text = R"(module Demo
def value: 4 / 0   +6;
value;
)";

  ASSERT_TRUE(server.didOpen(uri, "arithmetics", text));

  const auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(uri);
  const auto *diagnostic =
      find_diagnostic(diagnostics, "Division by zero is detected.");
  ASSERT_NE(diagnostic, nullptr);

  const auto expectedBegin = find_offset(text, "4 / 0");
  const auto expectedEnd = static_cast<pegium::TextOffset>(
      expectedBegin + std::string_view("4 / 0").size());

  EXPECT_EQ(diagnostic->begin, expectedBegin);
  EXPECT_EQ(diagnostic->end, expectedEnd);
}

TEST(ExampleParityTest,
     ArithmeticsHoverDoesNotCrashOnRecoveredDanglingPlusWithSpaces) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(arithmetics::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///arithmetics/recovery-dangling-plus.calc";
  const std::string text = R"(module calc
def a: 5;
def b: 3;
def c: a + b;
2 * c     +   ; // 16
b % 2; // 1
)";

  ASSERT_TRUE(server.didOpen(uri, "arithmetics", text));

  const auto plusOffset = find_offset(text, "+   ;");
  ASSERT_GT(plusOffset, 0u);

  ::lsp::HoverParams hoverParams{};
  hoverParams.textDocument.uri = ::lsp::Uri::parse(uri);
  hoverParams.position = position_from_offset(text, plusOffset);
  EXPECT_NO_THROW((void)server.getHoverContent(hoverParams));
}

TEST(ExampleParityTest, DomainmodelRenameUpdatesSimpleAndQualifiedReferences) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(domainmodel::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///domainmodel/example.dmodel";
  const std::string text = R"(package foo {
  entity Person {
  }
}
entity Holder {
  person: foo.Person
  many friends: Person
}
)";

  ASSERT_TRUE(server.didOpen(uri, "domain-model", text));

  const auto renameOffset = find_offset(text, "Person");
  ::lsp::RenameParams renameParams{};
  renameParams.textDocument.uri = ::lsp::Uri::parse(uri);
  renameParams.position = position_from_offset(text, renameOffset);
  renameParams.newName = "User";
  const auto edit = server.rename(renameParams);
  ASSERT_TRUE(edit.has_value());
  ASSERT_TRUE(edit->changes.contains(uri));

  bool replacedDeclaration = false;
  bool replacedQualifiedReference = false;
  bool replacedSimpleReference = false;
  for (const auto &change : edit->changes.at(uri)) {
    if (change.newText == "User") {
      replacedDeclaration = true;
      replacedSimpleReference = true;
    }
    if (change.newText == "foo.User") {
      replacedQualifiedReference = true;
    }
  }

  EXPECT_TRUE(replacedDeclaration);
  EXPECT_TRUE(replacedQualifiedReference);
  EXPECT_TRUE(replacedSimpleReference);
}

TEST(ExampleParityTest, RequirementsWarnWhenRequirementIsNotCovered) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(requirements::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///requirements/example.req";
  const std::string text = R"(environment dev: "Development"
req REQ "Uncovered requirement" applicable for dev
)";

  ASSERT_TRUE(server.didOpen(uri, "requirements-lang", text));

  const auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(uri);
  const auto *diagnostic =
      find_diagnostic(diagnostics, "not covered by a test");
  ASSERT_NE(diagnostic, nullptr);
  EXPECT_GT(diagnostic->begin, 0u);
  EXPECT_GT(diagnostic->end, diagnostic->begin);
}

TEST(ExampleParityTest, RequirementsWarnOnEnvironmentMismatchInTests) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(requirements::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string requirementsUri = "file:///requirements/example.req";
  const std::string requirementsText = R"(environment dev: "Development"
environment prod: "Production"
req REQ1 "Covered requirement" applicable for dev
)";
  const std::string testsUri = "file:///requirements/example.tst";
  const std::string testsText = R"(tst TEST1 tests REQ1 applicable for prod
)";

  ASSERT_TRUE(server.didOpen(requirementsUri, "requirements-lang",
                             requirementsText));
  ASSERT_TRUE(server.didOpen(testsUri, "tests-lang", testsText));

  const auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(testsUri);
  const auto *diagnostic =
      find_diagnostic(diagnostics, "references environment prod");
  ASSERT_NE(diagnostic, nullptr);
  EXPECT_GT(diagnostic->begin, 0u);
  EXPECT_GT(diagnostic->end, diagnostic->begin);
}

TEST(ExampleParityTest, StatemachineValidatesCapitalizationAndDuplicates) {
  pegium::services::SharedServices shared;
  ASSERT_TRUE(statemachine::services::register_language_services(shared));

  pegium::lsp::DefaultLanguageServer server(shared);
  const std::string uri = "file:///statemachine/example.statemachine";
  const std::string text = R"(statemachine TrafficLight
events tick tick
initialState red
state red
end
state Red
end
)";

  ASSERT_TRUE(server.didOpen(uri, "statemachine", text));
  const auto diagnostics = shared.workspace.workspaceManager->collectDiagnostics(uri);
  const auto *capitalization = find_diagnostic(
      diagnostics, "State name should start with a capital letter.");
  ASSERT_NE(capitalization, nullptr);
  EXPECT_GT(capitalization->begin, 0u);
  EXPECT_GT(capitalization->end, capitalization->begin);

  const auto *duplicate =
      find_diagnostic(diagnostics, "Duplicate identifier name: tick");
  ASSERT_NE(duplicate, nullptr);
  EXPECT_GT(duplicate->begin, 0u);
  EXPECT_GT(duplicate->end, duplicate->begin);
}
