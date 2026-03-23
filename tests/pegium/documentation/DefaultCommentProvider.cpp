#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

namespace pegium::documentation {
namespace {

using namespace pegium::parser;

struct DocumentationEntry : AstNode {
  string name;
};

struct DocumentationModel : AstNode {
  string name;
  vector<pointer<DocumentationEntry>> entries;
};

class DocumentationParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return ModelRule;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper =
      SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();

  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<DocumentationEntry> EntryRule{"Entry",
                                     "entry"_kw + assign<&DocumentationEntry::name>(ID)};

  Rule<DocumentationModel> ModelRule{
      "Model",
      option("module"_kw + assign<&DocumentationModel::name>(ID)) +
          some(append<&DocumentationModel::entries>(EntryRule))};
#pragma clang diagnostic pop
};

TEST(DefaultCommentProviderTest, ReturnsLeadingHiddenMultilineComment) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs",
                                                    {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("comment.docs"), "docs",
      "/** First entry */\n"
      "entry First\n"
      "entry Second\n");
  ASSERT_NE(document, nullptr);

  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->entries.size(), 2u);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.documentation.commentProvider, nullptr);

  EXPECT_EQ(services.documentation.commentProvider->getComment(
                *model->entries.front()),
            "/** First entry */");
  EXPECT_TRUE(services.documentation.commentProvider->getComment(
                  *model->entries.back())
                  .empty());
}

TEST(DefaultCommentProviderTest, ReturnsLeadingHiddenMultilineCommentForRootNode) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs",
                                                    {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("root-comment.docs"), "docs",
      "/** Root model */\n"
      "module Demo\n"
      "entry First\n");
  ASSERT_NE(document, nullptr);

  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.documentation.commentProvider, nullptr);

  EXPECT_EQ(services.documentation.commentProvider->getComment(*model),
            "/** Root model */");
}

} // namespace
} // namespace pegium::documentation
