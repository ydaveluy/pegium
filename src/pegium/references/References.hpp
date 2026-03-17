#pragma once

#include <optional>
#include <string_view>

#include <pegium/utils/Stream.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/AstDescriptions.hpp>

namespace pegium::references {

class References {
public:
  virtual ~References() noexcept = default;

  virtual std::optional<workspace::AstNodeDescription>
  findDeclarationAt(const workspace::Document &document,
                    TextOffset offset) const = 0;

  [[nodiscard]] virtual utils::stream<workspace::ReferenceDescriptionOrDeclaration>
  findReferencesAt(const workspace::Document &document, TextOffset offset,
                   bool includeDeclaration) const = 0;
};

} // namespace pegium::references
