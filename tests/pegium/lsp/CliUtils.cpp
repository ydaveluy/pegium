#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium {
namespace {

struct RegisteredLanguage {
  pegium::CoreServices *services = nullptr;
  test::FakeParser *parser = nullptr;
  test::FakeDocumentValidator *validator = nullptr;
};

std::filesystem::path make_temp_directory() {
  const auto suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  auto path = std::filesystem::temp_directory_path() /
              ("pegium-cli-tests-" + suffix);
  std::filesystem::create_directories(path);
  return path;
}

std::filesystem::path write_file(const std::filesystem::path &path,
                                 std::string_view content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
  return path;
}

RegisteredLanguage register_language(
    pegium::SharedCoreServices &shared, std::string languageId,
    std::vector<std::string> fileExtensions = {},
    std::vector<std::string> fileNames = {}) {
  auto languageServices = std::make_unique<pegium::CoreServices>(shared);
  pegium::installDefaultCoreServices(*languageServices);

  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  languageServices->parser = std::move(parser);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  languageServices->validation.documentValidator = std::move(validator);

  languageServices->languageMetaData.languageId = std::move(languageId);
  languageServices->languageMetaData.fileExtensions = std::move(fileExtensions);
  languageServices->languageMetaData.fileNames = std::move(fileNames);

  auto *servicesPtr = languageServices.get();
  shared.serviceRegistry->registerServices(std::move(languageServices));
  return {
      .services = servicesPtr,
      .parser = parserPtr,
      .validator = validatorPtr,
  };
}

TEST(CliUtilsTest, MakeSharedServicesReadiesStandaloneConfigurationProvider) {
  auto shared = cli::make_shared_services();

  ASSERT_NE(shared.workspace.configurationProvider, nullptr);
  EXPECT_TRUE(shared.workspace.configurationProvider->isReady());
}

TEST(CliUtilsTest, BuildDocumentFromPathReusesExistingDocumentForSameUri) {
  auto shared = cli::make_shared_services();
  const auto language =
      register_language(shared, "cli-language", {".cli"});

  const auto tempDirectory = make_temp_directory();
  const auto inputPath =
      write_file(tempDirectory / "demo.cli", "content");

  const auto firstDocument =
      cli::build_document_from_path(inputPath.string(), *language.services);
  const auto secondDocument =
      cli::build_document_from_path(inputPath.string(), *language.services);

  ASSERT_NE(firstDocument, nullptr);
  ASSERT_NE(secondDocument, nullptr);
  EXPECT_EQ(firstDocument, secondDocument);
  EXPECT_EQ(language.parser->parseCalls, 1u);

  const auto uri = utils::path_to_file_uri(inputPath.string());
  EXPECT_EQ(shared.workspace.documents->getDocuments(uri).size(), 1u);

  std::filesystem::remove_all(tempDirectory);
}

TEST(CliUtilsTest, BuildDocumentFromPathRejectsMissingFilesClearly) {
  auto shared = cli::make_shared_services();
  const auto language =
      register_language(shared, "cli-language", {".cli"});

  const auto tempDirectory = make_temp_directory();
  const auto missingPath = tempDirectory / "missing.cli";

  try {
    (void)cli::build_document_from_path(missingPath.string(), *language.services);
    FAIL() << "Expected invalid_argument";
  } catch (const std::invalid_argument &error) {
    EXPECT_NE(std::string(error.what()).find("does not exist."),
              std::string::npos);
  }

  std::filesystem::remove_all(tempDirectory);
}

TEST(CliUtilsTest, BuildDocumentFromPathRejectsUnexpectedExtensionsClearly) {
  auto shared = cli::make_shared_services();
  const auto language =
      register_language(shared, "cli-language", {".cli"});

  const auto tempDirectory = make_temp_directory();
  const auto inputPath =
      write_file(tempDirectory / "demo.txt", "content");

  try {
    (void)cli::build_document_from_path(inputPath.string(), *language.services);
    FAIL() << "Expected invalid_argument";
  } catch (const std::invalid_argument &error) {
    EXPECT_NE(
        std::string(error.what()).find(
            "Please choose a file with one of these extensions: .cli."),
        std::string::npos);
  }

  std::filesystem::remove_all(tempDirectory);
}

TEST(CliUtilsTest, BuildDocumentFromPathRejectsUnexpectedFileNamesClearly) {
  auto shared = cli::make_shared_services();
  const auto language =
      register_language(shared, "cli-language", {}, {"pegium.config"});

  const auto tempDirectory = make_temp_directory();
  const auto inputPath =
      write_file(tempDirectory / "other.config", "content");

  try {
    (void)cli::build_document_from_path(inputPath.string(), *language.services);
    FAIL() << "Expected invalid_argument";
  } catch (const std::invalid_argument &error) {
    EXPECT_NE(
        std::string(error.what()).find(
            "Please choose a file with one of these names: pegium.config."),
        std::string::npos);
  }

  std::filesystem::remove_all(tempDirectory);
}

TEST(CliUtilsTest, BuildDocumentFromPathSkipsValidationWhenRequested) {
  auto shared = cli::make_shared_services();
  const auto language =
      register_language(shared, "cli-language", {".cli"});

  const auto tempDirectory = make_temp_directory();
  const auto inputPath =
      write_file(tempDirectory / "demo.cli", "content");

  auto document = cli::build_document_from_path(inputPath.string(),
                                                *language.services, false);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(language.validator->validateCalls, 0u);

  std::filesystem::remove_all(tempDirectory);
}

} // namespace
} // namespace pegium
