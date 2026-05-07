#include <mydsl/ast.hpp>
#include <mydsl/core/Module.hpp>

#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/workspace/Document.hpp>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

// Tiny smoke test: parse `example/sample.mydsl` and assert that the default
// content yields two persons, two greetings, and that all greetings link to
// a real person. Designed to run without GTest so the template stays small.

int main(int /*argc*/, char *argv[]) {
  if (argv[0] == nullptr) {
    std::cerr << "argv[0] is null\n";
    return 1;
  }
  const std::string samplePath =
#ifdef MYDSL_SAMPLE_PATH
      MYDSL_SAMPLE_PATH;
#else
      "example/sample.mydsl";
#endif

  auto shared = pegium::cli::make_shared_services();
  auto services = mydsl::createMyDslServices(shared);
  auto &mydslServices = *services;
  shared.serviceRegistry->registerServices(std::move(services));

  auto document =
      pegium::cli::build_document_from_path(samplePath, mydslServices, true);

  if (pegium::cli::has_error_diagnostics(*document)) {
    pegium::cli::print_error_diagnostics(*document, std::cerr);
    return 1;
  }

  const auto *model =
      dynamic_cast<const mydsl::ast::Model *>(document->parseResult.value);
  if (model == nullptr) {
    std::cerr << "no Model root\n";
    return 1;
  }

  std::size_t personCount = 0;
  std::size_t greetingCount = 0;
  std::size_t resolvedGreetings = 0;
  for (const auto *element : model->elements) {
    if (dynamic_cast<const mydsl::ast::Person *>(element) != nullptr) {
      ++personCount;
    } else if (const auto *greeting =
                   dynamic_cast<const mydsl::ast::Greeting *>(element);
               greeting != nullptr) {
      ++greetingCount;
      if (greeting->person.get() != nullptr) {
        ++resolvedGreetings;
      }
    }
  }

  if (personCount != 2 || greetingCount != 2) {
    std::cerr << "expected 2 persons + 2 greetings, got " << personCount
              << " + " << greetingCount << "\n";
    return 1;
  }
  if (resolvedGreetings != greetingCount) {
    std::cerr << "some greetings did not link\n";
    return 1;
  }

  std::cout << "smoke ok\n";
  return 0;
}
