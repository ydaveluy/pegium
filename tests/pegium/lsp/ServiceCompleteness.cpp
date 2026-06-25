#include <gtest/gtest.h>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace pegium {
namespace {

// Exercises SharedServices::isComplete() (the LSP override) at each install
// stage. The override additionally requires the shared LSP runtime services, so
// it must report incomplete until installDefaultSharedLspServices has run — the
// invariant a virtual install-time assert previously got wrong.
TEST(LspServiceCompletenessTest, SharedServicesCompleteOnlyAfterBothInstalls) {
  auto shared = test::make_empty_shared_services();
  EXPECT_FALSE(shared->isComplete());

  installDefaultSharedCoreServices(*shared);
  // Core shared services are present, but the required LSP runtime services are
  // not installed yet, so the LSP override must still report incomplete.
  EXPECT_FALSE(shared->isComplete());

  installDefaultSharedLspServices(*shared);
  EXPECT_TRUE(shared->isComplete());
}

} // namespace
} // namespace pegium
