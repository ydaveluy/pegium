#include <gtest/gtest.h>

#include <statemachine/parser/Parser.hpp>
#include <statemachine/services/Module.hpp>

#include <pegium/ExampleTestSupport.hpp>

namespace statemachine::tests {
namespace {

TEST(StatemachineLanguageTest, ParsesSimpleStateMachine) {
  parser::StateMachineParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "statemachine Light\n"
      "events Start\n"
      "initialState Idle\n"
      "state Idle end\n",
      pegium::test::make_file_uri("machine.statemachine"), "statemachine");

  ASSERT_TRUE(document->parseSucceeded());
  auto *model =
      dynamic_cast<ast::Statemachine *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->states.size(), 1u);
}

TEST(StatemachineLanguageTest, LinksInitialStateTransitionAndActionReferences) {
  auto shared = pegium::test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(statemachine::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("linked.statemachine"),
      "statemachine",
      "statemachine Light\n"
      "events Start Stop\n"
      "commands Open Close\n"
      "initialState Idle\n"
      "state Idle actions { Open Close }\n"
      "Start => Running\n"
      "end\n"
      "state Running end\n");

  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<ast::Statemachine *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_TRUE(model->init);
  EXPECT_EQ(model->init->name, "Idle");
  ASSERT_EQ(model->states.size(), 2u);

  auto *idle = model->states.front().get();
  ASSERT_NE(idle, nullptr);
  ASSERT_EQ(idle->actions.size(), 2u);
  ASSERT_TRUE(idle->actions[0]);
  ASSERT_TRUE(idle->actions[1]);
  EXPECT_EQ(idle->actions[0]->name, "Open");
  EXPECT_EQ(idle->actions[1]->name, "Close");

  ASSERT_EQ(idle->transitions.size(), 1u);
  auto *transition = idle->transitions.front().get();
  ASSERT_NE(transition, nullptr);
  ASSERT_TRUE(transition->event);
  ASSERT_TRUE(transition->state);
  EXPECT_EQ(transition->event->name, "Start");
  EXPECT_EQ(transition->state->name, "Running");
}

TEST(StatemachineLanguageTest, RecoversMissingArrowCharacterInsideTransition) {
  parser::StateMachineParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "statemachine TrafficLight\n"
      "\n"
      "events\n"
      "    switchCapacity\n"
      "    next\n"
      "\n"
      "initialState PowerOff\n"
      "\n"
      "state PowerOff\n"
      "    switchCapacity > RedLight\n"
      "end\n"
      "\n"
      "state RedLight\n"
      "    switchCapacity => PowerOff\n"
      "    next => GreenLight\n"
      "end\n"
      "\n"
      "state YellowLight\n"
      "    switchCapacity => PowerOff\n"
      "    next => RedLight\n"
      "end\n"
      "\n"
      "state GreenLight\n"
      "    switchCapacity => PowerOff\n"
      "    next => YellowLight\n"
      "end\n",
      pegium::test::make_file_uri("recovery-arrow.statemachine"),
      "statemachine");

  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<ast::Statemachine *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->states.size(), 4u);
  EXPECT_TRUE(document->parseResult.recoveryReport.hasRecovered);
  EXPECT_FALSE(document->parseResult.parseDiagnostics.empty());
}

} // namespace
} // namespace statemachine::tests
