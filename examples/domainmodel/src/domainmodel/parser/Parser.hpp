#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <domainmodel/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace domainmodel::parser {

using namespace pegium::parser;

class DomainModelParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Domainmodel;
  }

  const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};

  Rule<ast::Feature> FeatureRule{
      "Feature",
      option(enable_if<&ast::Feature::many>("many"_kw.i())) +
          assign<&ast::Feature::name>(ID) + ":"_kw +
          assign<&ast::Feature::type>(QualifiedName)};

  Rule<ast::DataType> DataTypeRule{
      "DataType", "datatype"_kw.i() + assign<&ast::DataType::name>(ID)};

  Rule<ast::Entity> EntityRule{
      "Entity",
      "entity"_kw.i() + assign<&ast::Entity::name>(ID) +
          option("extends"_kw.i() +
                 assign<&ast::Entity::superType>(QualifiedName)) +
          "{"_kw + many(append<&ast::Entity::features>(FeatureRule)) +
          "}"_kw};

  Rule<ast::Type> TypeRule{"Type", DataTypeRule | EntityRule};

  Rule<ast::PackageDeclaration> PackageDeclarationRule{
      "PackageDeclaration",
      "package"_kw.i() + assign<&ast::PackageDeclaration::name>(QualifiedName) +
          "{"_kw +
          many(append<&ast::PackageDeclaration::elements>(AbstractElementRule)) +
          "}"_kw};

  Rule<ast::AbstractElement> AbstractElementRule{"AbstractElement",
                                                 PackageDeclarationRule |
                                                     TypeRule};

  Rule<ast::DomainModel> Domainmodel{
      "Domainmodel",
      some(append<&ast::DomainModel::elements>(AbstractElementRule))};
#pragma clang diagnostic pop
};

} // namespace domainmodel::parser
