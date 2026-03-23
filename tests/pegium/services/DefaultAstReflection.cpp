#include <gtest/gtest.h>

#include <ranges>
#include <typeindex>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/syntax-tree/DefaultAstReflection.hpp>

namespace pegium {
namespace {

using namespace pegium::parser;

struct BaseType : AstNode {};
struct MidType : BaseType {};
struct LeafType : MidType {};
struct OtherType : AstNode {};

class TransitiveReflectionParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return BaseRule;
  }

  Rule<LeafType> LeafRule{"Leaf", "leaf"_kw};
  Rule<MidType> MidRule{"Mid", LeafRule};
  Rule<BaseType> BaseRule{"Base", MidRule};
};

class KnownTypeReflectionParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  struct RootNode : AstNode {
    Reference<OtherType> ref;
  };
  Rule<RootNode> RootRule{"Root", "use"_kw + assign<&RootNode::ref>(ID)};
};

class LaterRegistrationLeafParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return MidRule;
  }

  Rule<LeafType> LeafRule{"Leaf", "leaf"_kw};
  Rule<MidType> MidRule{"Mid", LeafRule};
};

class LaterRegistrationBaseParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return BaseRule;
  }

  Rule<MidType> MidRule{"Mid", "mid"_kw};
  Rule<BaseType> BaseRule{"Base", MidRule};
};

TEST(DefaultAstReflectionTest, MatchesExactAndTransitiveSubtypes) {
  DefaultAstReflection reflection;
  TransitiveReflectionParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         reflection);

  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(BaseType)),
                                   std::type_index(typeid(BaseType))));
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(MidType)),
                                   std::type_index(typeid(BaseType))));
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(LeafType)),
                                   std::type_index(typeid(BaseType))));
  EXPECT_FALSE(reflection.isSubtype(std::type_index(typeid(BaseType)),
                                    std::type_index(typeid(LeafType))));
}

TEST(DefaultAstReflectionTest, RegisteredTypesAreExposedWithoutSubtypes) {
  DefaultAstReflection reflection;
  KnownTypeReflectionParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         reflection);

  const auto &knownTypes = reflection.getAllTypes();
  EXPECT_NE(std::ranges::find(knownTypes, std::type_index(typeid(OtherType))),
            knownTypes.end());
  EXPECT_FALSE(reflection.isSubtype(std::type_index(typeid(OtherType)),
                                    std::type_index(typeid(BaseType))));
}

TEST(DefaultAstReflectionTest, ReturnsAllSubTypesIncludingExactType) {
  DefaultAstReflection reflection;
  TransitiveReflectionParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         reflection);

  const auto &subtypes =
      reflection.getAllSubTypes(std::type_index(typeid(BaseType)));
  EXPECT_NE(std::ranges::find(subtypes, std::type_index(typeid(BaseType))),
            subtypes.end());
  EXPECT_NE(std::ranges::find(subtypes, std::type_index(typeid(MidType))),
            subtypes.end());
  EXPECT_NE(std::ranges::find(subtypes, std::type_index(typeid(LeafType))),
            subtypes.end());
}

TEST(DefaultAstReflectionTest, ReturnsStableEmptySetForUnknownSubtypeQuery) {
  DefaultAstReflection reflection;

  const auto &subtypes =
      reflection.getAllSubTypes(std::type_index(typeid(LeafType)));

  EXPECT_TRUE(subtypes.empty());
  EXPECT_EQ(&subtypes,
            &reflection.getAllSubTypes(std::type_index(typeid(LeafType))));
}

TEST(DefaultAstReflectionTest, ChecksRuntimeInstancesByReflectedType) {
  DefaultAstReflection reflection;
  TransitiveReflectionParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         reflection);

  LeafType leaf;
  BaseType base;

  EXPECT_TRUE(
      reflection.isInstance(leaf, std::type_index(typeid(pegium::AstNode))));
  EXPECT_TRUE(reflection.isInstance(leaf, std::type_index(typeid(BaseType))));
  EXPECT_TRUE(reflection.isInstance(leaf, std::type_index(typeid(MidType))));
  EXPECT_TRUE(reflection.isInstance(leaf, std::type_index(typeid(LeafType))));
  EXPECT_FALSE(reflection.isInstance(base, std::type_index(typeid(LeafType))));
}

TEST(DefaultAstReflectionTest, TreatsAstNodeAsImplicitRootSupertype) {
  DefaultAstReflection reflection;
  TransitiveReflectionParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         reflection);

  const auto astNodeType = std::type_index(typeid(pegium::AstNode));
  const auto &allTypes = reflection.getAllTypes();
  const auto &subtypes = reflection.getAllSubTypes(astNodeType);

  EXPECT_NE(std::ranges::find(allTypes, astNodeType), allTypes.end());
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(BaseType)),
                                   astNodeType));
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(MidType)),
                                   astNodeType));
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(LeafType)),
                                   astNodeType));
  EXPECT_NE(std::ranges::find(subtypes, astNodeType), subtypes.end());
  EXPECT_NE(std::ranges::find(subtypes, std::type_index(typeid(BaseType))),
            subtypes.end());
  EXPECT_NE(std::ranges::find(subtypes, std::type_index(typeid(MidType))),
            subtypes.end());
  EXPECT_NE(std::ranges::find(subtypes, std::type_index(typeid(LeafType))),
            subtypes.end());
}

TEST(DefaultAstReflectionTest, PropagatesTransitiveSubtypeOnLaterRegistration) {
  DefaultAstReflection reflection;
  LaterRegistrationLeafParser leafParser;
  LaterRegistrationBaseParser baseParser;
  bootstrapAstReflection(static_cast<const Parser &>(leafParser).getEntryRule(),
                         reflection);
  bootstrapAstReflection(static_cast<const Parser &>(baseParser).getEntryRule(),
                         reflection);

  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(LeafType)),
                                   std::type_index(typeid(BaseType))));
}

} // namespace
} // namespace pegium
