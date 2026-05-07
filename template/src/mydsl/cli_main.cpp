#include <mydsl/ast.hpp>
#include <mydsl/core/Module.hpp>

#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/workspace/Document.hpp>

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

int run(std::string_view path) {
  auto shared = pegium::cli::make_shared_services();
  auto services = mydsl::createMyDslServices(shared);
  auto &mydslServices = *services;
  shared.serviceRegistry->registerServices(std::move(services));

  auto document =
      pegium::cli::build_document_from_path(path, mydslServices, true);

  if (pegium::cli::has_error_diagnostics(*document)) {
    pegium::cli::print_error_diagnostics(*document, std::cerr);
    return 2;
  }

  const auto *model =
      dynamic_cast<const mydsl::ast::Model *>(document->parseResult.value);
  if (model == nullptr) {
    std::cerr << "Document did not produce a Model root.\n";
    return 2;
  }

  std::size_t personCount = 0;
  std::size_t greetingCount = 0;
  for (const auto *element : model->elements) {
    if (dynamic_cast<const mydsl::ast::Person *>(element) != nullptr) {
      ++personCount;
    } else if (dynamic_cast<const mydsl::ast::Greeting *>(element) !=
               nullptr) {
      ++greetingCount;
    }
  }

  std::cout << "Parsed " << personCount << " person(s) and " << greetingCount
            << " greeting(s).\n";
  for (const auto *element : model->elements) {
    const auto *greeting =
        dynamic_cast<const mydsl::ast::Greeting *>(element);
    if (greeting == nullptr) {
      continue;
    }
    const auto *target = greeting->person.get();
    std::cout << "  Hello -> "
              << (target != nullptr ? target->name
                                    : std::string{"<unresolved>"})
              << "\n";
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: mydsl-cli <file.mydsl>\n";
    return 1;
  }
  try {
    return run(argv[1]);
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 3;
  }
}
