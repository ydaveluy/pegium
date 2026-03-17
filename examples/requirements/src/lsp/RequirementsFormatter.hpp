#pragma once

#include <requirements/ast.hpp>

#include <pegium/lsp/AbstractFormatter.hpp>

namespace requirements::services::lsp {

class RequirementsFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit RequirementsFormatter(const pegium::services::Services &services);

protected:
  virtual void formatContact(pegium::lsp::FormattingBuilder &builder,
                             const ast::Contact *contact) const;
  virtual void formatRequirementModel(pegium::lsp::FormattingBuilder &builder,
                                      const ast::RequirementModel *model) const;
  virtual void formatEnvironment(pegium::lsp::FormattingBuilder &builder,
                                 const ast::Environment *environment) const;
  virtual void formatRequirement(pegium::lsp::FormattingBuilder &builder,
                                 const ast::Requirement *requirement) const;
};

class TestsFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit TestsFormatter(const pegium::services::Services &services);

protected:
  virtual void formatContact(pegium::lsp::FormattingBuilder &builder,
                             const ast::Contact *contact) const;
  virtual void formatTestModel(pegium::lsp::FormattingBuilder &builder,
                               const ast::TestModel *model) const;
  virtual void formatTest(pegium::lsp::FormattingBuilder &builder,
                          const ast::Test *test) const;
};

} // namespace requirements::services::lsp
