#pragma once

#include <requirements/ast.hpp>

#include <pegium/lsp/formatting/AbstractFormatter.hpp>

namespace requirements::services::lsp {

class RequirementsFormatter : public pegium::AbstractFormatter {
public:
  explicit RequirementsFormatter(const pegium::Services &services);

protected:
  virtual void formatContact(pegium::FormattingBuilder &builder,
                             const ast::Contact *contact) const;
  virtual void formatRequirementModel(pegium::FormattingBuilder &builder,
                                      const ast::RequirementModel *model) const;
  virtual void formatEnvironment(pegium::FormattingBuilder &builder,
                                 const ast::Environment *environment) const;
  virtual void formatRequirement(pegium::FormattingBuilder &builder,
                                 const ast::Requirement *requirement) const;
};

class TestsFormatter : public pegium::AbstractFormatter {
public:
  explicit TestsFormatter(const pegium::Services &services);

protected:
  virtual void formatContact(pegium::FormattingBuilder &builder,
                             const ast::Contact *contact) const;
  virtual void formatTestModel(pegium::FormattingBuilder &builder,
                               const ast::TestModel *model) const;
  virtual void formatTest(pegium::FormattingBuilder &builder,
                          const ast::Test *test) const;
};

} // namespace requirements::services::lsp
