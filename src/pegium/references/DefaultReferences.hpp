#pragma once

#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/references/References.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::references {

class DefaultReferences : public References,
                          protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  std::optional<workspace::AstNodeDescription>
  findDeclarationAt(const workspace::Document &document,
                    TextOffset offset) const override;

  utils::stream<workspace::ReferenceDescriptionOrDeclaration>
  findReferencesAt(const workspace::Document &document, TextOffset offset,
                   bool includeDeclaration) const override;

};

} // namespace pegium::references
