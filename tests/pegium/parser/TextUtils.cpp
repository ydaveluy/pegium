#include <gtest/gtest.h>
#include <pegium/parser/TextUtils.hpp>

using namespace pegium::parser;

TEST(TextUtilsTest, CreateCharacterRangeSupportsSingleValuesAndRanges) {
  auto lookup = createCharacterRange("a-c0-2_");

  EXPECT_TRUE(lookup[static_cast<unsigned char>('a')]);
  EXPECT_TRUE(lookup[static_cast<unsigned char>('b')]);
  EXPECT_TRUE(lookup[static_cast<unsigned char>('c')]);
  EXPECT_TRUE(lookup[static_cast<unsigned char>('0')]);
  EXPECT_TRUE(lookup[static_cast<unsigned char>('2')]);
  EXPECT_TRUE(lookup[static_cast<unsigned char>('_')]);
  EXPECT_FALSE(lookup[static_cast<unsigned char>('z')]);
}

TEST(TextUtilsTest, CharacterHelpersReturnExpectedValues) {
  EXPECT_FALSE(isWord('\0'));
  EXPECT_TRUE(isWord('A'));
  EXPECT_TRUE(isWord('_'));
  EXPECT_FALSE(isWord('-'));

  EXPECT_EQ(tolower('A'), 'a');
  EXPECT_EQ(tolower('z'), 'z');

  EXPECT_EQ(escape_char('\n'), "\\n");
  EXPECT_EQ(escape_char('\t'), "\\t");
  EXPECT_EQ(escape_char('\x01'), "\\x01");
}
