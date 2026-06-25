#include <gtest/gtest.h>

#include <utility>

#include <pegium/core/services/ServiceAccess.hpp>

namespace {

// Minimal stand-ins for the real container shape: a framework base, an
// added-services base, and a concrete container that derives from both — exactly
// the layout of `struct XCoreServices final : pegium::CoreServices,
// XAddedServices`. service_cast must recover both the concrete container
// (downcast) and the added-services base (sidecast) from a base reference, which
// is what the per-language asXCoreServices / asXAddedServices helpers rely on.
struct Base {
  Base() = default;
  Base(const Base &) = delete;
  Base &operator=(const Base &) = delete;
  virtual ~Base() = default;
};

struct AddedFeature {
  AddedFeature() = default;
  AddedFeature(const AddedFeature &) = delete;
  AddedFeature &operator=(const AddedFeature &) = delete;
  virtual ~AddedFeature() = default;
  int marker = 42;
};

struct Derived final : Base, AddedFeature {};
struct UnrelatedDerived final : Base {};

TEST(ServiceAccessTest, RecoversDerivedContainerFromBaseReference) {
  Derived derived;
  const Base &base = derived;
  EXPECT_EQ(pegium::service_cast<Derived>(base), &derived);
}

TEST(ServiceAccessTest, RecoversAddedServicesBaseFromBaseReference) {
  Derived derived;
  const Base &base = derived;
  const auto *added = pegium::service_cast<AddedFeature>(base);
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added, static_cast<const AddedFeature *>(&derived));
  EXPECT_EQ(added->marker, 42);
}

TEST(ServiceAccessTest, ReturnsNullForUnrelatedDynamicType) {
  UnrelatedDerived unrelated;
  const Base &base = unrelated;
  EXPECT_EQ(pegium::service_cast<Derived>(base), nullptr);
  EXPECT_EQ(pegium::service_cast<AddedFeature>(base), nullptr);
}

// A custom service inherits LanguageServiceMixin to capture a typed, non-owning
// back-reference to its language-specific services, reachable without a cast.
struct ServiceWithLanguageRef final
    : pegium::LanguageServiceMixin<AddedFeature> {
  explicit ServiceWithLanguageRef(const AddedFeature &services)
      : pegium::LanguageServiceMixin<AddedFeature>(services) {}

  [[nodiscard]] const AddedFeature &exposed() const { return languageServices; }
};

TEST(ServiceAccessTest, LanguageServiceMixinExposesTypedReference) {
  AddedFeature feature;
  ServiceWithLanguageRef service(feature);
  EXPECT_EQ(&service.exposed(), &feature);
  EXPECT_EQ(service.exposed().marker, 42);
}

static_assert(
    noexcept(pegium::service_cast<Derived>(std::declval<const Base &>())),
    "service_cast must be noexcept");

} // namespace
