#include <gtest/gtest.h>

#include <lsp/connection.h>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/core/observability/ObservabilitySink.hpp>

namespace pegium {
namespace {

TEST(LspObservabilityTest, InitializeSharedServicesForLanguageServerPublishesLogMessage) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);

  SharedServices shared;
  auto recorder = std::make_shared<test::RecordingObservabilitySink>();
  shared.observabilitySink = recorder;

  initializeSharedServicesForLanguageServer(shared, connection);

  shared.observabilitySink->publish(observability::Observation{
      .severity = observability::ObservationSeverity::Warning,
      .code = observability::ObservationCode::LanguageMappingCollision,
      .message = "collision",
      .languageId = "calc",
  });

  ASSERT_TRUE(recorder->waitForCount(1));
  const auto messages = test::parse_written_messages(stream.written());
  ASSERT_EQ(messages.size(), 1u);
  const auto payload = messages.front().object();
  EXPECT_EQ(payload.get("method").string(), "window/logMessage");
  const auto &params = payload.get("params").object();
  EXPECT_EQ(params.get("type").integer(), 2);
  EXPECT_NE(params.get("message").string().find("LanguageMappingCollision"),
            std::string::npos);
}

TEST(LspObservabilityTest,
     WorkspaceBootstrapFailuresPublishLogAndShowMessages) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);

  SharedServices shared;
  shared.observabilitySink = std::make_shared<test::RecordingObservabilitySink>();

  initializeSharedServicesForLanguageServer(shared, connection);

  shared.observabilitySink->publish(observability::Observation{
      .severity = observability::ObservationSeverity::Error,
      .code = observability::ObservationCode::WorkspaceBootstrapFailed,
      .message = "bootstrap failed",
      .uri = test::make_file_uri("workspace"),
  });

  const auto messages = test::parse_written_messages(stream.written());
  ASSERT_EQ(messages.size(), 2u);
  EXPECT_EQ(messages[0].object().get("method").string(), "window/logMessage");
  EXPECT_EQ(messages[1].object().get("method").string(), "window/showMessage");
  EXPECT_EQ(messages[0].object().get("params").object().get("type").integer(), 1);
  EXPECT_EQ(messages[1].object().get("params").object().get("type").integer(), 1);
}

TEST(LspObservabilityTest,
     InitializeSharedServicesForLanguageServerDoesNotDuplicateSinkOnReentry) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);

  SharedServices shared;
  shared.observabilitySink = std::make_shared<test::RecordingObservabilitySink>();

  initializeSharedServicesForLanguageServer(shared, connection);
  initializeSharedServicesForLanguageServer(shared, connection);

  shared.observabilitySink->publish(observability::Observation{
      .severity = observability::ObservationSeverity::Info,
      .code = observability::ObservationCode::ReferenceResolutionProblem,
      .message = "single log",
  });

  const auto messages = test::parse_written_messages(stream.written());
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(messages.front().object().get("method").string(),
            "window/logMessage");
}

} // namespace
} // namespace pegium
