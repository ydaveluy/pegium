#pragma once

#include <optional>

#include <pegium/workspace/Document.hpp>

namespace pegium::lsp {

struct ServiceRequirement {
  enum class Type { Document, Workspace };

  constexpr ServiceRequirement() noexcept = default;
  constexpr ServiceRequirement(workspace::DocumentState state) noexcept
      : type(Type::Document), state(state) {}
  constexpr ServiceRequirement(Type type,
                               workspace::DocumentState state) noexcept
      : type(type), state(state) {}

  Type type = Type::Document;
  workspace::DocumentState state = workspace::DocumentState::Changed;
};

namespace WorkspaceState {

inline constexpr ServiceRequirement Parsed{
    ServiceRequirement::Type::Workspace, workspace::DocumentState::Parsed};
inline constexpr ServiceRequirement IndexedContent{
    ServiceRequirement::Type::Workspace,
    workspace::DocumentState::IndexedContent};
inline constexpr ServiceRequirement ComputedScopes{
    ServiceRequirement::Type::Workspace,
    workspace::DocumentState::ComputedScopes};
inline constexpr ServiceRequirement Linked{
    ServiceRequirement::Type::Workspace, workspace::DocumentState::Linked};
inline constexpr ServiceRequirement IndexedReferences{
    ServiceRequirement::Type::Workspace,
    workspace::DocumentState::IndexedReferences};
inline constexpr ServiceRequirement Validated{
    ServiceRequirement::Type::Workspace, workspace::DocumentState::Validated};

} // namespace WorkspaceState

struct ServiceRequirements {
  std::optional<ServiceRequirement> CallHierarchyProvider;
  std::optional<ServiceRequirement> CodeActionProvider;
  std::optional<ServiceRequirement> CodeLensProvider;
  std::optional<ServiceRequirement> CompletionProvider;
  std::optional<ServiceRequirement> DeclarationProvider;
  std::optional<ServiceRequirement> DefinitionProvider;
  std::optional<ServiceRequirement> DocumentHighlightProvider;
  std::optional<ServiceRequirement> DocumentLinkProvider;
  std::optional<ServiceRequirement> DocumentSymbolProvider;
  std::optional<ServiceRequirement> FoldingRangeProvider;
  std::optional<ServiceRequirement> Formatter;
  std::optional<ServiceRequirement> HoverProvider;
  std::optional<ServiceRequirement> ImplementationProvider;
  std::optional<ServiceRequirement> InlayHintProvider;
  std::optional<ServiceRequirement> ReferencesProvider;
  std::optional<ServiceRequirement> RenameProvider;
  std::optional<ServiceRequirement> SemanticTokenProvider;
  std::optional<ServiceRequirement> SignatureHelp;
  std::optional<ServiceRequirement> TypeHierarchyProvider;
  std::optional<ServiceRequirement> TypeProvider;
  std::optional<ServiceRequirement> WorkspaceSymbolProvider;
};

} // namespace pegium::lsp
