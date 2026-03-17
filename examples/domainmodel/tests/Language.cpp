#include <gtest/gtest.h>

#include <domainmodel/services/Module.hpp>
#include <domainmodel/parser/Parser.hpp>

#include <pegium/ExampleTestSupport.hpp>

namespace domainmodel::tests {
namespace {

TEST(DomainModelLanguageTest, ParsesEntityModel) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "package blog {\n"
      "  entity Person {}\n"
      "}\n",
      pegium::test::make_file_uri("language.dmodel"), "domain-model");

  ASSERT_TRUE(document->parseSucceeded());
  auto *model = dynamic_cast<ast::DomainModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->elements.size(), 1u);
}

TEST(DomainModelLanguageTest, ResolvesTypeReferenceToEntitySubtype) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(domainmodel::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("types.dmodel"), "domain-model",
      "entity Person {}\n"
      "entity Blog {\n"
      "  owner: Person\n"
      "}\n");
  ASSERT_NE(document, nullptr);
  ASSERT_TRUE(document->parseSucceeded());

  auto *model =
      dynamic_cast<ast::DomainModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->elements.size(), 2u);

  auto *blog = dynamic_cast<ast::Entity *>(model->elements[1].get());
  ASSERT_NE(blog, nullptr);
  ASSERT_EQ(blog->features.size(), 1u);
  ASSERT_NE(blog->features.front(), nullptr);
  ASSERT_NE(blog->features.front()->type.get(), nullptr);
  auto *person = dynamic_cast<ast::Entity *>(blog->features.front()->type.get());
  ASSERT_NE(person, nullptr);
  EXPECT_EQ(person->name, "Person");
}

TEST(DomainModelLanguageTest,
     RecoveryKeepsBuildingModelAfterDuplicateFeatureColon) {
  parser::DomainModelParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "entity Blog {\n"
      "    title: String\n"
      "}\n"
      "\n"
      "entity Post {\n"
      "    title: String\n"
      "    content:: String\n"
      "    many comments: Comment\n"
      "}\n"
      "\n"
      "entity Comment {\n"
      "    content: String\n"
      "}\n",
      pegium::test::make_file_uri("recovery-blog.dmodel"), "domain-model");

  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<ast::DomainModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  EXPECT_GE(model->elements.size(), 3u);
  EXPECT_FALSE(document->parseResult.parseDiagnostics.empty());
}

} // namespace
} // namespace domainmodel::tests
