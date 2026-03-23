#include <gtest/gtest.h>

#include <pegium/ParseJsonTestSupport.hpp>

#include "JsonConverterTestSupport.hpp"

namespace pegium::converter {
namespace {
TEST(AstJsonConverterTest, ConvertsBuiltAstToReferenceJson) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  ASSERT_TRUE(test_support::register_language(*shared));

  const auto document = test_support::open_reference_document(*shared);
  ASSERT_NE(document, nullptr);

  const auto expected = R"({
  "$type": "Model",
  "greetings": [
    {
      "$type": "Greeting",
      "person": {
        "$ref": "#/persons@0",
        "$refText": "John"
      }
    }
  ],
  "persons": [
    {
      "$type": "Person",
      "name": "John"
    }
  ]
})";

  pegium::test::ExpectAst(*document, expected);
}

} // namespace
} // namespace pegium::converter
