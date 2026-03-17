#include "lsp/DomainModelFormatter.hpp"

namespace domainmodel::services::lsp {

void DomainModelFormatter::formatDomainModel(pegium::lsp::FormattingBuilder &builder,
                                             const ast::DomainModel *model) const {
  auto formatter = builder.getNodeFormatter(model);
  formatter.properties<&ast::DomainModel::elements>().prepend(noIndent);
}

void DomainModelFormatter::formatPackageDeclaration(
    pegium::lsp::FormattingBuilder &builder,
    const ast::PackageDeclaration *package) const {
  auto formatter = builder.getNodeFormatter(package);
  formatter.keyword("package").append(oneSpace);
  const auto openBrace = formatter.keyword("{");
  const auto closeBrace = formatter.keyword("}");
  formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
}

void DomainModelFormatter::formatEntity(pegium::lsp::FormattingBuilder &builder,
                                        const ast::Entity *entity) const {
  auto formatter = builder.getNodeFormatter(entity);
  formatter.keyword("entity").append(oneSpace);
  if (entity->superType.has_value()) {
    formatter.keyword("extends").prepend(oneSpace).append(oneSpace);
  }
  const auto openBrace = formatter.keyword("{");
  const auto closeBrace = formatter.keyword("}");
  formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
}

void DomainModelFormatter::formatDataType(pegium::lsp::FormattingBuilder &builder,
                                          const ast::DataType *dataType) const {
  auto formatter = builder.getNodeFormatter(dataType);
  formatter.keyword("datatype").append(oneSpace);
}

void DomainModelFormatter::formatFeature(pegium::lsp::FormattingBuilder &builder,
                                         const ast::Feature *feature) const {
  auto formatter = builder.getNodeFormatter(feature);
  if (feature->many) {
    formatter.keyword("many").append(oneSpace);
  }
  formatter.keyword(":").prepend(noSpace).append(oneSpace);
}

void DomainModelFormatter::formatLineComment(HiddenNodeFormatter &comment) const {
  comment.replace(AbstractFormatter::formatLineComment(comment));
}

DomainModelFormatter::DomainModelFormatter(
    const pegium::services::Services &services)
    : AbstractFormatter(services) {
  on<ast::DomainModel>(&DomainModelFormatter::formatDomainModel);
  on<ast::PackageDeclaration>(&DomainModelFormatter::formatPackageDeclaration);
  on<ast::Entity>(&DomainModelFormatter::formatEntity);
  on<ast::DataType>(&DomainModelFormatter::formatDataType);
  on<ast::Feature>(&DomainModelFormatter::formatFeature);
  onHidden("SL_COMMENT", &DomainModelFormatter::formatLineComment);
}

} // namespace domainmodel::services::lsp
