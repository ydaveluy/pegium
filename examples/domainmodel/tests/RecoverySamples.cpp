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
    ASSERT_NE(blog, nullptr)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
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
    ASSERT_NE(blog, nullptr)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
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

  if (sample.label == "consecutive_keyword_typos.dmodel") {
    // Two consecutive `entit` typos (each requires a 1-char fuzzy
    // replace to `entity`) followed by a clean `entity Post extends
    // HasAuthor`. Before the cost-vs-progress ratio guards on the
    // family-redundancy filters, the second typo's recovery
    // cascaded — the OrderedChoice promoted a "delete the whole
    // bad block" candidate over the cheap fuzzy because the
    // delete-cascade reached further. The fix requires both typos
    // to recover independently as fuzzy replaces, yielding three
    // entities in the model.
    ASSERT_EQ(model->elements.size(), 3u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    auto *blog = dynamic_cast<ast::Entity *>(model->elements[0].get());
    auto *hasAuthor = dynamic_cast<ast::Entity *>(model->elements[1].get());
    auto *post = dynamic_cast<ast::Entity *>(model->elements[2].get());
    ASSERT_NE(blog, nullptr);
    ASSERT_NE(hasAuthor, nullptr);
    ASSERT_NE(post, nullptr);
    EXPECT_EQ(blog->name, "Blog");
    EXPECT_EQ(hasAuthor->name, "HasAuthor");
    EXPECT_EQ(post->name, "Post");
    ASSERT_TRUE(post->superType.has_value());
    EXPECT_EQ(post->superType->getRefText(), "HasAuthor");
    return;
  }

  if (sample.label == "far_entity_keyword_typo.dmodel") {
    ASSERT_EQ(model->elements.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_elements(*model);
    if (auto *blog =
            dynamic_cast<ast::Entity *>(model->elements.front().get())) {
      EXPECT_EQ(blog->name, "Blog");
      return;
    }
    auto *pkg =
        dynamic_cast<ast::PackageDeclaration *>(model->elements.front().get());
    ASSERT_NE(pkg, nullptr) << sample.label << " :: " << parseDump << " :: "
                            << summarize_elements(*model);
    EXPECT_EQ(pkg->name, "Blog");
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

TEST(DomainModelRecoveryRegressionTest,
     RecoversFirstEntityTypoAndLaterExtendsTypoInSameDocument) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "entit Blog {\n"
      "    title: String\n"
      "    date: complex.Date\n"
      "    many posts: Post\n"
      "}\n"
      "\n"
      "entity HasAuthor {\n"
      "    author: String\n"
      "}\n"
      "\n"
      "entity Post extends HasAuthor {\n"
      "    title: String\n"
      "    content: String\n"
      "    many comments: Comment\n"
      "}\n"
      "\n"
      "entity Comment extend HasAuthor {\n"
      "    content: String\n"
      "}\n",
      pegium::test::make_file_uri("first-entit-later-extend.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 4u)
      << parseDump << " :: " << summarize_elements(*model);

  auto *blog = dynamic_cast<ast::Entity *>(model->elements[0].get());
  auto *hasAuthor = dynamic_cast<ast::Entity *>(model->elements[1].get());
  auto *post = dynamic_cast<ast::Entity *>(model->elements[2].get());
  auto *comment = dynamic_cast<ast::Entity *>(model->elements[3].get());
  ASSERT_NE(blog, nullptr);
  ASSERT_NE(hasAuthor, nullptr);
  ASSERT_NE(post, nullptr);
  ASSERT_NE(comment, nullptr);

  EXPECT_EQ(blog->name, "Blog");
  EXPECT_EQ(hasAuthor->name, "HasAuthor");
  EXPECT_EQ(post->name, "Post");
  EXPECT_EQ(comment->name, "Comment");
  ASSERT_TRUE(post->superType.has_value()) << parseDump;
  ASSERT_TRUE(comment->superType.has_value()) << parseDump;
  EXPECT_EQ(post->superType->getRefText(), "HasAuthor");
  EXPECT_EQ(comment->superType->getRefText(), "HasAuthor");
  EXPECT_EQ(blog->features.size(), 3u);
  EXPECT_EQ(comment->features.size(), 1u);
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversPackageTypoAndNestedEntityMissingOpenBrace) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "packag blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person \n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri(
          "package-typo-nested-entity-missing-open-brace.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u)
      << parseDump << "\n" << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_EQ(pkg->elements.size(), 6u);
  auto *person = dynamic_cast<ast::Entity *>(pkg->elements[2].get());
  auto *author = dynamic_cast<ast::Entity *>(pkg->elements[3].get());
  auto *post = dynamic_cast<ast::Entity *>(pkg->elements[4].get());
  ASSERT_NE(person, nullptr);
  ASSERT_NE(author, nullptr);
  ASSERT_NE(post, nullptr);
  EXPECT_EQ(person->features.size(), 2u);
  ASSERT_EQ(author->features.size(), 1u);
  EXPECT_EQ(author->features.front()->name, "pseudonym");
  ASSERT_EQ(post->features.size(), 3u);
  EXPECT_EQ(post->features[0]->name, "title");
  EXPECT_EQ(post->features[1]->name, "author");
  EXPECT_EQ(post->features[1]->type.getRefText(), "Author");
  EXPECT_EQ(post->features[2]->name, "tags");
  EXPECT_TRUE(post->features[2]->many);
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversNestedEntityMissingOpenBraceWithoutEarlierRecovery) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person \n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri(
          "nested-entity-missing-open-brace.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u)
      << parseDump << "\n" << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_EQ(pkg->elements.size(), 6u);
  auto *author = dynamic_cast<ast::Entity *>(pkg->elements[3].get());
  ASSERT_NE(author, nullptr);
  EXPECT_EQ(author->name, "Author");
  ASSERT_EQ(author->features.size(), 1u);
  EXPECT_EQ(author->features.front()->name, "pseudonym");
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversPackageAndNestedEntityMissingOpenBraces) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core \n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person \n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person {\n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri(
          "package-and-nested-entity-missing-open-braces.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u)
      << parseDump << "\n" << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_GE(pkg->elements.size(), 4u);
  auto *person = dynamic_cast<ast::Entity *>(pkg->elements[2].get());
  ASSERT_NE(person, nullptr);
  EXPECT_EQ(person->name, "Person");
  ASSERT_EQ(person->features.size(), 2u);
  EXPECT_EQ(person->features[0]->name, "name");
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversTwoNestedEntitiesMissingOpenBraces) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person \n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person \n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri(
          "two-nested-entities-missing-open-braces.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u)
      << parseDump << "\n" << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_GE(pkg->elements.size(), 4u);
  auto *person = dynamic_cast<ast::Entity *>(pkg->elements[2].get());
  auto *author = dynamic_cast<ast::Entity *>(pkg->elements[3].get());
  ASSERT_NE(person, nullptr);
  ASSERT_NE(author, nullptr);
  EXPECT_EQ(person->name, "Person");
  EXPECT_EQ(author->name, "Author");
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversEntityMissingOpenBraceAndManyKeywordTypo) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person {\n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post \n"
      "    title: String\n"
      "    author: Author\n"
      "    man tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri(
          "entity-missing-open-brace-many-keyword-typo.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u)
      << parseDump << "\n" << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_GE(pkg->elements.size(), 5u);
  auto *post = dynamic_cast<ast::Entity *>(pkg->elements[4].get());
  ASSERT_NE(post, nullptr);
  ASSERT_EQ(post->features.size(), 3u);
  ASSERT_NE(post->features[2], nullptr);
  EXPECT_TRUE(post->features[2]->many);
  EXPECT_EQ(post->features[2]->name, "tags");
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversFeatureColonTypoAndMissingTrailingBraces) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person {\n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  \n",
      pegium::test::make_file_uri(
          "feature-colon-typo-missing-trailing-braces.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;

  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u) << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_EQ(pkg->elements.size(), 6u);
  auto *person = dynamic_cast<ast::Entity *>(pkg->elements[2].get());
  auto *author = dynamic_cast<ast::Entity *>(pkg->elements[3].get());
  auto *post = dynamic_cast<ast::Entity *>(pkg->elements[4].get());
  ASSERT_NE(person, nullptr);
  ASSERT_NE(author, nullptr);
  ASSERT_NE(post, nullptr);
  EXPECT_EQ(person->features.size(), 2u);
  ASSERT_EQ(author->features.size(), 1u);
  EXPECT_EQ(author->features.front()->name, "pseudonym");
  ASSERT_EQ(post->features.size(), 3u);
  EXPECT_EQ(post->features[1]->type.getRefText(), "Author");
  EXPECT_EQ(post->features[2]->name, "tags");
  EXPECT_TRUE(post->features[2]->many);
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversPackageHeaderAndNestedKeywordDamage) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "packag blog.core \n"
      "  datatyp String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entity Author extends Person {\n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post \n"
      "    title: String\n"
      "    author: Author\n"
      "    man tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person \n"
      "    body String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri("package-header-nested-damage.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversLargeKeywordFuzzCombination) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "packag blog.core \n"
      "  datatype String\n"
      "  datatyp Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  }\n"
      "\n"
      "  entit Author extends Person {\n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post \n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags String\n"
      "  \n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri("large-keyword-fuzz-combination.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversAdjacentMissingCloseAndOpenBraces) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  \n"
      "\n"
      "  entity Author extends Person \n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags: String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri("adjacent-missing-close-open-braces.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;
  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u) << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_EQ(pkg->elements.size(), 6u);
  auto *person = dynamic_cast<ast::Entity *>(pkg->elements[2].get());
  auto *author = dynamic_cast<ast::Entity *>(pkg->elements[3].get());
  auto *post = dynamic_cast<ast::Entity *>(pkg->elements[4].get());
  ASSERT_NE(person, nullptr);
  ASSERT_NE(author, nullptr);
  ASSERT_NE(post, nullptr);
  EXPECT_EQ(person->features.size(), 2u);
  ASSERT_EQ(author->features.size(), 1u);
  EXPECT_EQ(author->features.front()->name, "pseudonym");
  ASSERT_EQ(post->features.size(), 3u);
  EXPECT_EQ(post->features[1]->type.getRefText(), "Author");
  EXPECT_EQ(post->features[2]->name, "tags");
  EXPECT_TRUE(post->features[2]->many);
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversMissingCloseBraceAndLaterFeatureColon) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth: blog.core.Date\n"
      "  \n"
      "\n"
      "  entity Author extends Person {\n"
      "    pseudonym: String\n"
      "  }\n"
      "\n"
      "  entity Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags String\n"
      "  }\n"
      "\n"
      "  entity Comment extends Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri("missing-close-later-feature-colon.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;
  auto *model = dynamic_cast<const ast::DomainModel *>(parsed.value.get());
  ASSERT_NE(model, nullptr) << parseDump;
  ASSERT_EQ(model->elements.size(), 1u)
      << parseDump << "\n" << summarize_elements(*model);
  auto *pkg = dynamic_cast<ast::PackageDeclaration *>(model->elements[0].get());
  ASSERT_NE(pkg, nullptr);
  ASSERT_EQ(pkg->elements.size(), 6u);
  auto *post = dynamic_cast<ast::Entity *>(pkg->elements[4].get());
  ASSERT_NE(post, nullptr);
  ASSERT_EQ(post->features.size(), 3u);
  EXPECT_EQ(post->features[1]->type.getRefText(), "Author");
  EXPECT_EQ(post->features[2]->name, "tags");
  EXPECT_TRUE(post->features[2]->many);
}

TEST(DomainModelRecoveryRegressionTest,
     RecoversNestedEntityTyposWithMissingIntermediateClose) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog.core {\n"
      "  datatype String\n"
      "  datatype Date\n"
      "\n"
      "  entity Person {\n"
      "    name: String\n"
      "    birth blog.core.Date\n"
      "  }\n"
      "\n"
      "  entit Author extends Person {\n"
      "    pseudonym: String\n"
      "  \n"
      "\n"
      "  entit Post {\n"
      "    title: String\n"
      "    author: Author\n"
      "    many tags String\n"
      "  }\n"
      "\n"
      "  entity Comment extend Person {\n"
      "    body: String\n"
      "    parent: Post\n"
      "  }\n"
      "}\n",
      pegium::test::make_file_uri(
          "nested-entity-typos-missing-close.dmodel"),
      "domain-model");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << parseDump;
}

} // namespace
} // namespace domainmodel::tests
