#include <gtest/gtest.h>

#include <domainmodel/parser/Parser.hpp>

#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace domainmodel::tests {
namespace {

std::string summarize_elements(const ast::DomainModel &model) {
  std::string summary;
  for (const auto &element : model.elements) {
    if (!summary.empty()) {
      summary += " | ";
    }
    if (const auto *entity = dynamic_cast<const ast::Entity *>(element.get());
        entity != nullptr) {
      summary += "entity:";
      summary += entity->name;
      continue;
    }
    if (const auto *dataType =
            dynamic_cast<const ast::DataType *>(element.get());
        dataType != nullptr) {
      summary += "datatype:";
      summary += dataType->name;
      continue;
    }
    if (const auto *package =
            dynamic_cast<const ast::PackageDeclaration *>(element.get());
        package != nullptr) {
      summary += "package:";
      summary += package->name;
      continue;
    }
    summary += "other";
  }
  return summary;
}

void validate_sample(const pegium::test::NamedSampleFile &sample,
                     const pegium::workspace::Document &document) {
  const auto &parsed = document.parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << sample.label << " :: " << parseDump;

  if (sample.label == "duplicate_feature_colon.dmodel") {
    EXPECT_GE(model->elements.size(), 2u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    return;
  }

  if (sample.label == "close_missing_entity_keyword_char.dmodel") {
    ASSERT_EQ(model->elements.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *blog = dynamic_cast<ast::Entity *>(model->elements.front().get());
    ASSERT_NE(blog, nullptr);
    EXPECT_EQ(blog->name, "Blog");
    return;
  }

  if (sample.label == "close_missing_extends_keyword_char.dmodel") {
    ASSERT_EQ(model->elements.size(), 2u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *post = dynamic_cast<ast::Entity *>(model->elements[1].get());
    ASSERT_NE(post, nullptr);
    EXPECT_EQ(post->name, "Post");
    ASSERT_TRUE(post->superType.has_value());
    EXPECT_EQ(post->superType->getRefText(), "Blog");
    return;
  }

  if (sample.label == "missing_feature_colon.dmodel") {
    ASSERT_EQ(model->elements.size(), 2u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *blog = dynamic_cast<ast::Entity *>(model->elements[0].get());
    auto *comment = dynamic_cast<ast::Entity *>(model->elements[1].get());
    ASSERT_NE(blog, nullptr);
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(blog->name, "Blog");
    EXPECT_EQ(comment->name, "Comment");
    EXPECT_EQ(blog->features.size(), 2u);
    EXPECT_EQ(comment->features.size(), 1u);
    ASSERT_NE(blog->features[1], nullptr);
    EXPECT_EQ(blog->features[1]->name, "author");
    EXPECT_EQ(blog->features[1]->type.getRefText(), "Person");
    return;
  }

  if (sample.label == "missing_many_feature_colon.dmodel") {
    ASSERT_EQ(model->elements.size(), 2u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *post = dynamic_cast<ast::Entity *>(model->elements[0].get());
    auto *comment = dynamic_cast<ast::Entity *>(model->elements[1].get());
    ASSERT_NE(post, nullptr);
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(post->name, "Post");
    EXPECT_EQ(comment->name, "Comment");
    ASSERT_EQ(post->features.size(), 2u);
    ASSERT_NE(post->features[0], nullptr);
    EXPECT_TRUE(post->features[0]->many);
    EXPECT_EQ(post->features[0]->name, "comments");
    EXPECT_EQ(post->features[0]->type.getRefText(), "Comment");
    return;
  }

  if (sample.label == "missing_entity_open_brace.dmodel") {
    ASSERT_EQ(model->elements.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *blog = dynamic_cast<ast::Entity *>(model->elements.front().get());
    ASSERT_NE(blog, nullptr);
    EXPECT_EQ(blog->name, "Blog");
    ASSERT_EQ(blog->features.size(), 1u);
    ASSERT_NE(blog->features.front(), nullptr);
    EXPECT_EQ(blog->features.front()->name, "title");
    EXPECT_EQ(blog->features.front()->type.getRefText(), "String");
    return;
  }

  if (sample.label == "long_garbage_feature_line.dmodel") {
    ASSERT_EQ(model->elements.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *blog = dynamic_cast<ast::Entity *>(model->elements.front().get());
    ASSERT_NE(blog, nullptr);
    EXPECT_EQ(blog->name, "Blog");
    ASSERT_EQ(blog->features.size(), 1u);
    ASSERT_NE(blog->features.front(), nullptr);
    EXPECT_EQ(blog->features.front()->name, "title");
    EXPECT_EQ(blog->features.front()->type.getRefText(), "String");
    return;
  }

}

std::vector<pegium::test::NamedSampleFile> recovery_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "recovery-samples",
      {".dmodel"});
}

class DomainModelRecoverySampleTest
    : public ::testing::TestWithParam<pegium::test::NamedSampleFile> {};

TEST(DomainModelRecoverySampleCorpusTest, IsNotEmpty) {
  EXPECT_FALSE(recovery_samples().empty());
}

TEST_P(DomainModelRecoverySampleTest, RecoversCompletely) {
  const auto &sample = GetParam();

  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser, pegium::test::read_text_file(sample.path),
      pegium::test::make_file_uri(sample.label), "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << sample.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << sample.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered)
      << sample.label << " :: " << parseDump;
  EXPECT_FALSE(parsed.parseDiagnostics.empty())
      << sample.label << " :: " << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << sample.label << " :: " << parseDump;
  validate_sample(sample, *document);
}

INSTANTIATE_TEST_SUITE_P(
    RecoverySamples, DomainModelRecoverySampleTest,
    ::testing::ValuesIn(recovery_samples()),
    [](const ::testing::TestParamInfo<pegium::test::NamedSampleFile> &info) {
      return info.param.testName;
    });

} // namespace
} // namespace domainmodel::tests
