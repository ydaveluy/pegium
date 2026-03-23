#pragma once

#include <domainmodel/ast.hpp>

#include <pegium/lsp/formatting/AbstractFormatter.hpp>

namespace domainmodel::services::lsp {

class DomainModelFormatter : public pegium::AbstractFormatter {
public:
  explicit DomainModelFormatter(const pegium::Services &services);

protected:
  virtual void formatDomainModel(pegium::FormattingBuilder &builder,
                                 const ast::DomainModel *model) const;
  virtual void formatPackageDeclaration(
      pegium::FormattingBuilder &builder,
      const ast::PackageDeclaration *package) const;
  virtual void formatEntity(pegium::FormattingBuilder &builder,
                            const ast::Entity *entity) const;
  virtual void formatDataType(pegium::FormattingBuilder &builder,
                              const ast::DataType *dataType) const;
  virtual void formatFeature(pegium::FormattingBuilder &builder,
                             const ast::Feature *feature) const;
  virtual void formatMultilineComment(HiddenNodeFormatter &comment) const;
  virtual void formatLineComment(HiddenNodeFormatter &comment) const;
};

} // namespace domainmodel::services::lsp
