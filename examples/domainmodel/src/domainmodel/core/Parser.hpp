#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <domainmodel/core/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace domainmodel::parser {

using namespace pegium::parser;

class DomainModelParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

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
  // terminal SL_COMMENT returns string_view: ('//' (!&('\n' | '\r\n' | '\r' | !.) .)* &('\n' | '\r\n' | '\r' | !.));
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  // terminal ML_COMMENT returns string_view: ('/*' (!'*/' .)* '*/');
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  // terminal ID returns string: ([A-Z_a-z] [0-9A-Z_a-z]*);
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  // QualifiedName returns string: (ID ('.' ID)*);
  Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};

  // Feature returns Feature: (many?='many'i? name=ID ':' type=QualifiedName);
  Rule<ast::Feature> FeatureRule{
      "Feature",
      option(enable_if<&ast::Feature::many>("many"_kw.i())) +
          assign<&ast::Feature::name>(ID) + ":"_kw +
          assign<&ast::Feature::type>(QualifiedName)};

  // DataType returns DataType: ('datatype'i name=ID);
  Rule<ast::DataType> DataTypeRule{
      "DataType", "datatype"_kw.i() + assign<&ast::DataType::name>(ID)};

  // Entity returns Entity: ('entity'i name=ID ('extends'i superType=QualifiedName)? '{' features+=Feature* '}');
  Rule<ast::Entity> EntityRule{
      "Entity",
      "entity"_kw.i() + assign<&ast::Entity::name>(ID) +
          option("extends"_kw.i() +
                 assign<&ast::Entity::superType>(QualifiedName)) +
          "{"_kw + many(append<&ast::Entity::features>(FeatureRule)) +
          "}"_kw};

  // Type returns Type: (DataType | Entity);
  Rule<ast::Type> TypeRule{"Type", DataTypeRule | EntityRule};

  // PackageDeclaration returns PackageDeclaration: ('package'i name=QualifiedName '{' elements+=AbstractElement* '}');
  Rule<ast::PackageDeclaration> PackageDeclarationRule{
      "PackageDeclaration",
      "package"_kw.i() + assign<&ast::PackageDeclaration::name>(QualifiedName) +
          "{"_kw +
          many(append<&ast::PackageDeclaration::elements>(AbstractElementRule)) +
          "}"_kw};

  // AbstractElement returns AbstractElement: (PackageDeclaration | Type);
  Rule<ast::AbstractElement> AbstractElementRule{"AbstractElement",
                                                 PackageDeclarationRule |
                                                     TypeRule};

  // Domainmodel returns DomainModel: elements+=AbstractElement+;
  Rule<ast::DomainModel> Domainmodel{
      "Domainmodel",
      some(append<&ast::DomainModel::elements>(AbstractElementRule))};
#pragma clang diagnostic pop
};

} // namespace domainmodel::parser
