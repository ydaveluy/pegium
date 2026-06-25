#include <@PEGIUM_NEW_LANGUAGE_ID@/core/Module.hpp>
#include <pegium/cli/CliUtils.hpp>

#include <gtest/gtest.h>

TEST(@PEGIUM_NEW_CLASS@Parsing, SampleParsesWithoutErrors) {
  auto sharedServices = pegium::cli::make_shared_services();
  auto &shared = *sharedServices;
  auto services = @PEGIUM_NEW_LANGUAGE_ID@::create@PEGIUM_NEW_CLASS@Services(shared);
  auto &langServices = *services;
  shared.serviceRegistry->registerServices(std::move(services));

  auto document = pegium::cli::build_document_from_path(
      PEGIUM_NEW_SAMPLE_PATH, langServices);
  EXPECT_FALSE(pegium::cli::has_error_diagnostics(*document));
}
