#include "lsp/DomainModelFormatter.hpp"

namespace domainmodel::lsp {

namespace {

[[nodiscard]] bool is_doc_comment(std::string_view text) noexcept {
  return text.starts_with("/**");
}

} // namespace

void DomainModelFormatter::formatDomainModel(pegium::FormattingBuilder &builder,
                                             const ast::DomainModel *model) const {
  auto formatter = builder.getNodeFormatter(model);
  formatter.properties<&ast::DomainModel::elements>().prepend(noIndent);
}

void DomainModelFormatter::formatPackageDeclaration(
    pegium::FormattingBuilder &builder,
    const ast::PackageDeclaration *package) const {
  auto formatter = builder.getNodeFormatter(package);
  formatter.keyword("package").append(oneSpace);
  const auto openBrace = formatter.keyword("{");
  const auto closeBrace = formatter.keyword("}");
  formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
}

void DomainModelFormatter::formatEntity(pegium::FormattingBuilder &builder,
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

void DomainModelFormatter::formatDataType(pegium::FormattingBuilder &builder,
                                          const ast::DataType *dataType) const {
  auto formatter = builder.getNodeFormatter(dataType);
  formatter.keyword("datatype").append(oneSpace);
}

void DomainModelFormatter::formatFeature(pegium::FormattingBuilder &builder,
                                         const ast::Feature *feature) const {
  auto formatter = builder.getNodeFormatter(feature);
  if (feature->many) {
    formatter.keyword("many").append(oneSpace);
  }
  formatter.keyword(":").prepend(noSpace).append(oneSpace);
}

void DomainModelFormatter::formatMultilineComment(
    HiddenNodeFormatter &comment) const {
  const auto docComment = is_doc_comment(comment.text());
  comment.replace(AbstractFormatter::formatMultilineComment(
      comment.text(),
      {.start = docComment ? "/**" : "/*",
       .end = "*/",
       .newLineStart = docComment ? " *" : "",
       .tagStart = docComment ? std::optional<std::string>{"@"} : std::nullopt}));
}

void DomainModelFormatter::formatLineComment(HiddenNodeFormatter &comment) const {
  comment.replace(AbstractFormatter::formatLineComment(comment));
}

DomainModelFormatter::DomainModelFormatter(
    const pegium::Services &services)
    : AbstractFormatter(services) {
  on<ast::DomainModel>(&DomainModelFormatter::formatDomainModel);
  on<ast::PackageDeclaration>(&DomainModelFormatter::formatPackageDeclaration);
  on<ast::Entity>(&DomainModelFormatter::formatEntity);
  on<ast::DataType>(&DomainModelFormatter::formatDataType);
  on<ast::Feature>(&DomainModelFormatter::formatFeature);
  onHidden("ML_COMMENT", &DomainModelFormatter::formatMultilineComment);
  onHidden("SL_COMMENT", &DomainModelFormatter::formatLineComment);
}

} // namespace domainmodel::lsp
