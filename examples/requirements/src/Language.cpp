#include <requirements/parser/Parser.hpp>

#include <utility>

namespace requirements {

std::string decode_quoted_string(std::string_view text) {
  if (text.size() < 2) {
    return std::string(text);
  }

  const char quote = text.front();
  if ((quote != '\'' && quote != '"') || text.back() != quote) {
    return std::string(text);
  }

  std::string value;
  value.reserve(text.size() - 2);
  for (std::size_t i = 1; i + 1 < text.size(); ++i) {
    char c = text[i];
    if (c == '\\' && i + 2 < text.size()) {
      const char escaped = text[++i];
      switch (escaped) {
      case 'n':
        value.push_back('\n');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case 't':
        value.push_back('\t');
        break;
      case '\\':
      case '\'':
      case '"':
        value.push_back(escaped);
        break;
      default:
        value.push_back(escaped);
        break;
      }
      continue;
    }
    value.push_back(c);
  }

  return value;
}

} // namespace requirements

namespace requirements::parser {

std::unique_ptr<pegium::services::Services>
make_requirements_language_services(
    const pegium::services::SharedServices &sharedServices,
    std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser = std::make_unique<const RequirementsParser>(*services);
  return services;
}

std::unique_ptr<pegium::services::Services>
make_tests_language_services(
    const pegium::services::SharedServices &sharedServices,
    std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser = std::make_unique<const TestsParser>(*services);
  return services;
}

} // namespace requirements::parser
