#include <gtest/gtest.h>

#include <limits>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/Diagnostics.hpp>

namespace pegium::lsp {
namespace {

TEST(DiagnosticsTest, PublishDiagnosticsSerializesExtendedDiagnosticFields) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  workspace::DocumentDiagnosticsSnapshot snapshot{
      .uri = test::make_file_uri("diagnostics.test"),
      .version = std::numeric_limits<std::int64_t>::max(),
      .text = "alpha\nbeta\n",
      .diagnostics =
          {{
              .severity = services::DiagnosticSeverity::Warning,
              .message = "Use a better name",
              .source = "pegium-test",
              .code = std::int64_t{std::numeric_limits<std::int64_t>::max()},
              .codeDescription = "https://example.test/diagnostic",
              .tags = {
                  services::DiagnosticTag::Deprecated,
                  services::DiagnosticTag::Unnecessary,
              },
              .relatedInformation =
                  {{
                      .uri = {},
                      .message = "Related entry",
                      .begin = 0,
                      .end = 5,
                  }},
              .data = services::JsonValue::Object{
                  {"category", "style"},
                  {"fixable", true},
              },
              .begin = 6,
              .end = 10,
          }},
  };

  publish_diagnostics(&handler, snapshot);

  const auto message = test::parse_last_written_message(stream.written()).object();
  EXPECT_EQ(message.get("method").string(),
            ::lsp::notifications::TextDocument_PublishDiagnostics::Method);

  const auto &params = message.get("params").object();
  EXPECT_EQ(params.get("uri").string(), snapshot.uri);
  EXPECT_EQ(params.get("version").integer(),
            std::numeric_limits<::lsp::json::Integer>::max());

  const auto &diagnostics = params.get("diagnostics").array();
  ASSERT_EQ(diagnostics.size(), 1u);
  const auto &diagnostic = diagnostics.front().object();

  EXPECT_EQ(diagnostic.get("message").string(), "Use a better name");
  EXPECT_EQ(diagnostic.get("source").string(), "pegium-test");
  EXPECT_EQ(diagnostic.get("code").integer(),
            std::numeric_limits<::lsp::json::Integer>::max());

  const auto &codeDescription = diagnostic.get("codeDescription").object();
  EXPECT_EQ(codeDescription.get("href").string(),
            "https://example.test/diagnostic");

  const auto &tags = diagnostic.get("tags").array();
  ASSERT_EQ(tags.size(), 2u);
  EXPECT_EQ(tags[0].integer(), 2);
  EXPECT_EQ(tags[1].integer(), 1);

  const auto &relatedInformation =
      diagnostic.get("relatedInformation").array();
  ASSERT_EQ(relatedInformation.size(), 1u);
  const auto &relatedEntry = relatedInformation.front().object();
  EXPECT_EQ(relatedEntry.get("message").string(), "Related entry");
  EXPECT_EQ(
      relatedEntry.get("location").object().get("uri").string(),
      snapshot.uri);

  const auto &data = diagnostic.get("data").object();
  EXPECT_EQ(data.get("category").string(), "style");
  EXPECT_TRUE(data.get("fixable").boolean());

  const auto &range = diagnostic.get("range").object();
  EXPECT_EQ(range.get("start").object().get("line").integer(), 1);
  EXPECT_EQ(range.get("start").object().get("character").integer(), 0);
  EXPECT_EQ(range.get("end").object().get("character").integer(), 4);
}

TEST(DiagnosticsTest, PublishDiagnosticsSkipsInvalidCodeDescriptionUri) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  workspace::DocumentDiagnosticsSnapshot snapshot{
      .uri = test::make_file_uri("diagnostics-invalid-uri.test"),
      .text = "alpha",
      .diagnostics =
          {{
              .message = "bad uri",
              .codeDescription = "not a uri",
          }},
  };

  publish_diagnostics(&handler, snapshot);

  const auto message = test::parse_last_written_message(stream.written()).object();
  const auto &diagnostics =
      message.get("params").object().get("diagnostics").array();
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_FALSE(diagnostics.front().object().contains("codeDescription"));
}

} // namespace
} // namespace pegium::lsp
