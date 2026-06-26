#include <gtest/gtest.h>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/AstReflection.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/TextDocument.hpp>

#include <memory>
#include <typeindex>

namespace pegium {
namespace {

// AstNode <- VisibilityElement <- Member <- Field, single C++ inheritance.
// A grammar would name `Field` (the concrete rule result) and `Member` (a
// supertype it is assigned to), but `VisibilityElement` is a purely C++ base
// the grammar never mentions — exactly the shape that used to make a cast to it
// return nullptr through a reflection-attached arena.
struct VisibilityElement : AstNode {};
struct Member : VisibilityElement {};
struct Field : Member {};
struct UnrelatedNode : AstNode {};

class AstReflectionCastTest : public ::testing::Test {
protected:
  void SetUp() override {
    // The grammar registers the concrete node and the supertype it assigns to;
    // it never names the abstract VisibilityElement base.
    reflection.registerSubtype(typeid(Field), typeid(Member));

    textDocument = test::make_text_document(
        test::make_file_uri("reflection-cast.test"), "test", "x", 1);
    document = std::make_shared<workspace::Document>(textDocument);
    cst = std::make_unique<RootCstNode>(text::TextSnapshot::copy("x"));
    arena = std::make_unique<AstArena>(*cst);
    arena->attachDocument(*document, &reflection);
    field = arena->create<Field>();
  }

  AstReflection reflection;
  std::shared_ptr<workspace::TextDocument> textDocument;
  std::shared_ptr<workspace::Document> document;
  std::unique_ptr<RootCstNode> cst;
  std::unique_ptr<AstArena> arena;
  Field *field = nullptr;
};

TEST_F(AstReflectionCastTest, IsKnownReflectsRegistration) {
  EXPECT_TRUE(reflection.isKnown(typeid(Field)));
  EXPECT_TRUE(reflection.isKnown(typeid(Member)));
  EXPECT_FALSE(reflection.isKnown(typeid(VisibilityElement)));
  EXPECT_FALSE(reflection.isKnown(typeid(UnrelatedNode)));
}

TEST_F(AstReflectionCastTest, CastToRegisteredTypesUsesReflectionTable) {
  EXPECT_EQ(ast_ptr_cast<Field>(field), field);
  EXPECT_EQ(ast_ptr_cast<Member>(field), static_cast<Member *>(field));
  EXPECT_TRUE(is_a<Member>(field));
}

TEST_F(AstReflectionCastTest,
       CastToAbstractBaseTheGrammarNeverNamesFallsBackToDynamicCast) {
  // VisibilityElement is a real C++ base of Field but absent from the grammar
  // reflection, so isSubtype cannot answer for it. Without the fallback this
  // returned nullptr; dynamic_cast knows the inheritance and succeeds.
  auto *visibility = ast_ptr_cast<VisibilityElement>(field);
  EXPECT_NE(visibility, nullptr);
  EXPECT_EQ(visibility, static_cast<VisibilityElement *>(field));
  EXPECT_TRUE(is_a<VisibilityElement>(field));
}

TEST_F(AstReflectionCastTest, FallbackDoesNotOvermatchUnrelatedTypes) {
  // UnrelatedNode is also absent from the reflection, but dynamic_cast must
  // still reject it — the fallback widens what can be answered, not what
  // matches. (ast_ptr_cast<UnrelatedNode> would not even compile here: its
  // static_cast requires a static inheritance relationship, so a sideways cast
  // is a compile error rather than a runtime miss. is_a has no such cast.)
  EXPECT_FALSE(is_a<UnrelatedNode>(field));
}

} // namespace
} // namespace pegium
