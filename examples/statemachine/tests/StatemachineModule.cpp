#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>

#include <statemachine/services/Module.hpp>

#include "../src/lsp/StatemachineFormatter.hpp"
#include "../src/validation/StatemachineValidator.hpp"

#include <pegium/ExampleTestSupport.hpp>
#include <pegium/services/Services.hpp>

namespace statemachine::tests {
namespace {

std::string apply_text_edits(const pegium::workspace::Document &document,
                             std::vector<::lsp::TextEdit> edits) {
  auto text = document.text();
  std::sort(edits.begin(), edits.end(),
            [&document](const auto &left, const auto &right) {
              return document.positionToOffset(left.range.start) >
                     document.positionToOffset(right.range.start);
            });
  for (const auto &edit : edits) {
    const auto begin = document.positionToOffset(edit.range.start);
    const auto end = document.positionToOffset(edit.range.end);
    text.replace(begin, end - begin, edit.newText);
  }
  return text;
}

TEST(StatemachineModuleTest, SplitsValidationIntoDedicatedClass) {
  static_assert(std::is_class_v<
                statemachine::services::validation::StatemachineValidator>);
  SUCCEED();
}

TEST(StatemachineModuleTest, InstallsFormatterOverride) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      statemachine::services::create_language_services(*shared, "statemachine");

  ASSERT_NE(languageServices, nullptr);
  EXPECT_NE(dynamic_cast<statemachine::services::lsp::StatemachineFormatter *>(
                languageServices->lsp.formatter.get()),
            nullptr);
}

TEST(StatemachineModuleTest, ValidatorWarnsOnLowerCaseStateName) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(statemachine::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("machine.statemachine"),
      "statemachine",
      "statemachine Light\n"
      "events Start\n"
      "initialState idle\n"
      "state idle end\n");

  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(pegium::test::has_diagnostic_message(
      *document, "State name should start with a capital letter"));
}

TEST(StatemachineModuleTest, FormatterFormatsCompactMachine) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      statemachine::services::create_language_services(*shared, "statemachine");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("statemachine-format.statemachine"),
      "statemachine",
      "statemachine Light events Start Stop commands Open Close initialState Idle "
      "state Idle actions{Open Close} Start=>Running end state Running end");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("statemachine");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "statemachine Light\n"
            "events\n"
            "  Start\n"
            "  Stop\n"
            "commands\n"
            "  Open\n"
            "  Close\n"
            "initialState Idle\n"
            "\n"
            "state Idle actions { Open Close }\n"
            "  Start => Running\n"
            "end\n"
            "\n"
            "state Running end");
}

} // namespace
} // namespace statemachine::tests
