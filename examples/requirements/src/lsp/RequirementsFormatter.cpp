#include "lsp/RequirementsFormatter.hpp"

namespace requirements::services::lsp {
void RequirementsFormatter::formatContact(pegium::FormattingBuilder &builder,
                                          const ast::Contact *contact) const {
  auto formatter = builder.getNodeFormatter(contact);
  formatter.keyword(":").prepend(noSpace).append(oneSpace);
}

void RequirementsFormatter::formatRequirementModel(
    pegium::FormattingBuilder &builder,
    const ast::RequirementModel *model) const {
  auto formatter = builder.getNodeFormatter(model);
  formatter.properties<&ast::RequirementModel::contact,
                       &ast::RequirementModel::environments,
                       &ast::RequirementModel::requirements>()
      .slice(1)
      .prepend(newLine);
}

void RequirementsFormatter::formatEnvironment(
    pegium::FormattingBuilder &builder,
    const ast::Environment *environment) const {
  auto formatter = builder.getNodeFormatter(environment);
  formatter.keyword("environment").append(oneSpace);
  formatter.keyword(":").prepend(noSpace).append(oneSpace);
}

void RequirementsFormatter::formatRequirement(
    pegium::FormattingBuilder &builder,
    const ast::Requirement *requirement) const {
  auto formatter = builder.getNodeFormatter(requirement);
  formatter.keyword("req").append(oneSpace);
  if (!requirement->environments.empty()) {
    formatter.keyword("applicable").prepend(oneSpace).append(oneSpace);
    formatter.keyword("for").append(oneSpace);
    formatSeparatedList(formatter.keywords(","));
  }
}

void TestsFormatter::formatContact(pegium::FormattingBuilder &builder,
                                   const ast::Contact *contact) const {
  auto formatter = builder.getNodeFormatter(contact);
  formatter.keyword(":").prepend(noSpace).append(oneSpace);
}

void TestsFormatter::formatTestModel(pegium::FormattingBuilder &builder,
                                     const ast::TestModel *model) const {
  auto formatter = builder.getNodeFormatter(model);
  formatter.properties<&ast::TestModel::contact, &ast::TestModel::tests>()
      .slice(1)
      .prepend(newLine);
}

void TestsFormatter::formatTest(pegium::FormattingBuilder &builder,
                                const ast::Test *test) const {
  auto formatter = builder.getNodeFormatter(test);
  formatter.keyword("tst").append(oneSpace);

  if (test->testFile.has_value()) {
    formatter.keyword("testfile").prepend(oneSpace);
    formatter.keyword("=").prepend(oneSpace).append(oneSpace);
  }

  formatter.keyword("tests").prepend(oneSpace).append(oneSpace);
  formatSeparatedList(formatter.keywords(","));

  if (!test->environments.empty()) {
    formatter.keyword("applicable").prepend(oneSpace).append(oneSpace);
    formatter.keyword("for").append(oneSpace);
  }
}

RequirementsFormatter::RequirementsFormatter(
    const pegium::Services &services)
    : AbstractFormatter(services) {
  on<ast::RequirementModel>(&RequirementsFormatter::formatRequirementModel);
  on<ast::Contact>(&RequirementsFormatter::formatContact);
  on<ast::Environment>(&RequirementsFormatter::formatEnvironment);
  on<ast::Requirement>(&RequirementsFormatter::formatRequirement);
}

TestsFormatter::TestsFormatter(const pegium::Services &services)
    : AbstractFormatter(services) {
  on<ast::TestModel>(&TestsFormatter::formatTestModel);
  on<ast::Contact>(&TestsFormatter::formatContact);
  on<ast::Test>(&TestsFormatter::formatTest);
}

} // namespace requirements::services::lsp
