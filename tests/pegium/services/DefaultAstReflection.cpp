#include <gtest/gtest.h>

#include <typeindex>

#include <pegium/syntax-tree/DefaultAstReflection.hpp>

namespace pegium {
namespace {

struct BaseType {};
struct MidType : BaseType {};
struct LeafType : MidType {};
struct OtherType {};

TEST(DefaultAstReflectionTest, MatchesExactAndTransitiveSubtypes) {
  DefaultAstReflection reflection;
  reflection.registerSubtype(std::type_index(typeid(MidType)),
                             std::type_index(typeid(BaseType)));
  reflection.registerSubtype(std::type_index(typeid(LeafType)),
                             std::type_index(typeid(MidType)));

  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(BaseType)),
                                   std::type_index(typeid(BaseType))));
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(MidType)),
                                   std::type_index(typeid(BaseType))));
  EXPECT_TRUE(reflection.isSubtype(std::type_index(typeid(LeafType)),
                                   std::type_index(typeid(BaseType))));
  EXPECT_FALSE(reflection.isSubtype(std::type_index(typeid(BaseType)),
                                    std::type_index(typeid(LeafType))));
}

TEST(DefaultAstReflectionTest, RemembersExplicitNonSubtypes) {
  DefaultAstReflection reflection;
  reflection.registerNonSubtype(std::type_index(typeid(LeafType)),
                                std::type_index(typeid(OtherType)));

  EXPECT_EQ(reflection.lookupSubtype(std::type_index(typeid(LeafType)),
                                     std::type_index(typeid(OtherType))),
            std::optional<bool>(false));
  EXPECT_FALSE(reflection.isSubtype(std::type_index(typeid(LeafType)),
                                    std::type_index(typeid(OtherType))));
}

TEST(DefaultAstReflectionTest, PropagatesTransitiveSubtypeOnLaterRegistration) {
  DefaultAstReflection reflection;
  reflection.registerSubtype(std::type_index(typeid(LeafType)),
                             std::type_index(typeid(MidType)));
  reflection.registerSubtype(std::type_index(typeid(MidType)),
                             std::type_index(typeid(BaseType)));

  EXPECT_EQ(reflection.lookupSubtype(std::type_index(typeid(LeafType)),
                                     std::type_index(typeid(BaseType))),
            std::optional<bool>(true));
}

TEST(DefaultAstReflectionTest, LaterSubtypeOverridesKnownNonSubtype) {
  DefaultAstReflection reflection;
  reflection.registerNonSubtype(std::type_index(typeid(LeafType)),
                                std::type_index(typeid(BaseType)));
  reflection.registerSubtype(std::type_index(typeid(LeafType)),
                             std::type_index(typeid(MidType)));
  reflection.registerSubtype(std::type_index(typeid(MidType)),
                             std::type_index(typeid(BaseType)));

  EXPECT_EQ(reflection.lookupSubtype(std::type_index(typeid(LeafType)),
                                     std::type_index(typeid(BaseType))),
            std::optional<bool>(true));
}

} // namespace
} // namespace pegium
