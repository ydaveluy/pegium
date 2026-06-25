#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/Errors.hpp>

namespace pegium {
namespace {

// #9: the shared container has its own completeness check.
TEST(ServiceCompletenessTest, SharedCoreServicesCompleteOnlyAfterInstall) {
  auto shared = test::make_empty_shared_core_services();
  EXPECT_FALSE(shared->isComplete());
  installDefaultSharedCoreServices(*shared);
  EXPECT_TRUE(shared->isComplete());
}

// #8 + #6/#7: makeDefaultCoreServices installs every default service except the
// parser, which firstMissingService() then reports by name.
TEST(ServiceCompletenessTest, MakeDefaultCoreServicesLeavesOnlyParserMissing) {
  auto shared = test::make_empty_shared_core_services();
  installDefaultSharedCoreServices(*shared);

  auto services =
      makeDefaultCoreServices<test::TestCoreServices>(*shared, "lang");
  EXPECT_EQ(services->firstMissingService(),
            std::optional<std::string_view>("parser"));
  EXPECT_FALSE(services->isComplete());

  services->parser = std::make_unique<test::FakeParser>();
  EXPECT_EQ(services->firstMissingService(), std::nullopt);
  EXPECT_TRUE(services->isComplete());
}

// #6/#7: firstMissingService names a specific non-parser slot, and isComplete
// derives from it.
TEST(ServiceCompletenessTest, FirstMissingServiceNamesAnUninstalledService) {
  auto shared = test::make_empty_shared_core_services();
  installDefaultSharedCoreServices(*shared);

  // Has a parser (FakeParser) but no default language services installed yet.
  auto services = test::make_uninstalled_core_services(*shared, "lang");
  EXPECT_EQ(services->firstMissingService(),
            std::optional<std::string_view>("documentation.commentProvider"));

  installDefaultCoreServices(*services);
  EXPECT_EQ(services->firstMissingService(), std::nullopt);
  EXPECT_TRUE(services->isComplete());
}

// #7: registration failure reports WHICH service is missing, not just the
// language id.
TEST(ServiceCompletenessTest, RegisterServicesNamesTheMissingService) {
  auto shared = test::make_empty_shared_core_services();
  installDefaultSharedCoreServices(*shared);

  auto services = test::make_uninstalled_core_services(*shared, "lang");
  try {
    shared->serviceRegistry->registerServices(std::move(services));
    FAIL() << "expected ServiceRegistrationError";
  } catch (const utils::ServiceRegistrationError &error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("missing service 'documentation.commentProvider'"),
              std::string::npos)
        << message;
  }
}

} // namespace
} // namespace pegium
