#pragma once

#include <domainmodel/ast.hpp>

#include <pegium/lsp/AbstractFormatter.hpp>

namespace domainmodel::services::lsp {

class DomainModelFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit DomainModelFormatter(const pegium::services::Services &services);

protected:
  virtual void formatDomainModel(pegium::lsp::FormattingBuilder &builder,
                                 const ast::DomainModel *model) const;
  virtual void formatPackageDeclaration(
      pegium::lsp::FormattingBuilder &builder,
      const ast::PackageDeclaration *package) const;
  virtual void formatEntity(pegium::lsp::FormattingBuilder &builder,
                            const ast::Entity *entity) const;
  virtual void formatDataType(pegium::lsp::FormattingBuilder &builder,
                              const ast::DataType *dataType) const;
  virtual void formatFeature(pegium::lsp::FormattingBuilder &builder,
                             const ast::Feature *feature) const;
  virtual void formatLineComment(HiddenNodeFormatter &comment) const;
};

} // namespace domainmodel::services::lsp
