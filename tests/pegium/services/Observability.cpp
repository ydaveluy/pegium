#include <gtest/gtest.h>

#include <thread>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/observability/ObservationFormat.hpp>
#include <pegium/core/observability/ObservabilitySinks.hpp>

namespace pegium::observability {
namespace {

TEST(ObservabilityTest, InstallDefaultSharedCoreServicesInstallsDefaultSink) {
  auto shared = test::make_empty_shared_core_services();

  services::installDefaultSharedCoreServices(*shared);

  EXPECT_NE(shared->observabilitySink, nullptr);
}

TEST(ObservabilityTest, InstallDefaultSharedCoreServicesPreservesCustomSink) {
  auto shared = test::make_empty_shared_core_services();
  auto customSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = customSink;

  services::installDefaultSharedCoreServices(*shared);

  EXPECT_EQ(shared->observabilitySink, customSink);
}

TEST(ObservabilityTest, FormatObservationProducesStableSingleLineText) {
  const auto formatted = detail::format_observation(Observation{
      .severity = ObservationSeverity::Warning,
      .code = ObservationCode::LanguageMappingCollision,
      .message = "multi-line\nmessage",
      .uri = "file:///tmp/sample.test",
      .languageId = "sample",
      .category = "fast",
      .documentId = 7,
      .state = workspace::DocumentState::ComputedScopes,
  });

  EXPECT_NE(formatted.find("warning"), std::string::npos);
  EXPECT_NE(formatted.find("LanguageMappingCollision"), std::string::npos);
  EXPECT_NE(formatted.find("multi-line message"), std::string::npos);
  EXPECT_NE(formatted.find("uri=file:///tmp/sample.test"), std::string::npos);
  EXPECT_NE(formatted.find("languageId=sample"), std::string::npos);
  EXPECT_NE(formatted.find("category=fast"), std::string::npos);
  EXPECT_NE(formatted.find("documentId=7"), std::string::npos);
  EXPECT_NE(formatted.find("state=ComputedScopes"), std::string::npos);
  EXPECT_EQ(formatted.find('\n'), std::string::npos);
}

TEST(ObservabilityTest, StderrSinkPublishesFormattedLine) {
  StderrObservabilitySink sink;

  testing::internal::CaptureStderr();
  sink.publish(Observation{
      .severity = ObservationSeverity::Error,
      .code = ObservationCode::WorkspaceBootstrapFailed,
      .message = "bootstrap failed",
      .uri = "file:///tmp/workspace",
  });
  const auto output = testing::internal::GetCapturedStderr();

  EXPECT_NE(output.find("error WorkspaceBootstrapFailed: bootstrap failed"),
            std::string::npos);
  EXPECT_NE(output.find("uri=file:///tmp/workspace"), std::string::npos);
}

TEST(ObservabilityTest, FanoutSinkPublishesToAllChildrenAndAvoidsDuplicates) {
  auto first = std::make_shared<test::RecordingObservabilitySink>();
  auto second = std::make_shared<test::RecordingObservabilitySink>();
  FanoutObservabilitySink sink({first});

  sink.addSink(first);
  sink.addSink(second);
  sink.publish(Observation{
      .severity = ObservationSeverity::Info,
      .code = ObservationCode::ReferenceResolutionProblem,
      .message = "resolved",
  });

  ASSERT_TRUE(first->waitForCount(1));
  ASSERT_TRUE(second->waitForCount(1));
  EXPECT_EQ(sink.snapshot().size(), 2u);
  EXPECT_EQ(first->observations().size(), 1u);
  EXPECT_EQ(second->observations().size(), 1u);
}

TEST(ObservabilityTest, FanoutSinkSupportsConcurrentPublish) {
  auto recorder = std::make_shared<test::RecordingObservabilitySink>();
  FanoutObservabilitySink sink({recorder});

  constexpr std::size_t threadCount = 4;
  constexpr std::size_t messagesPerThread = 25;
  std::vector<std::thread> threads;
  threads.reserve(threadCount);

  for (std::size_t index = 0; index < threadCount; ++index) {
    threads.emplace_back([&sink, index]() {
      for (std::size_t messageIndex = 0; messageIndex < messagesPerThread;
           ++messageIndex) {
        sink.publish(Observation{
            .severity = ObservationSeverity::Trace,
            .code = ObservationCode::ReferenceResolutionProblem,
            .message = "message-" + std::to_string(index) + "-" +
                       std::to_string(messageIndex),
        });
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  ASSERT_TRUE(recorder->waitForCount(threadCount * messagesPerThread));
  EXPECT_EQ(recorder->observations().size(), threadCount * messagesPerThread);
}

} // namespace
} // namespace pegium::observability
