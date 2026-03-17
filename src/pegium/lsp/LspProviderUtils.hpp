#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_set>
#include <vector>

#include <lsp/types.h>

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/IndexManager.hpp>
#include <pegium/workspace/TextDocument.hpp>

namespace pegium::workspace {
class Documents;
}

namespace pegium::lsp::detail {

struct LocationData {
  workspace::DocumentId documentId = workspace::InvalidDocumentId;
  TextOffset begin = 0;
  TextOffset end = 0;
};

struct WorkspaceTextEdit {
  TextOffset begin = 0;
  TextOffset end = 0;
  std::string newText;
};

struct WorkspaceEditData {
  std::unordered_map<workspace::DocumentId, std::vector<WorkspaceTextEdit>> changes;

  [[nodiscard]] bool empty() const noexcept { return changes.empty(); }
};

struct TokenSpan {
  TextOffset begin = 0;
  TextOffset end = 0;
  std::string_view text{};
};

struct DocumentHighlightData {
  TextOffset begin = 0;
  TextOffset end = 0;
  ::lsp::DocumentHighlightKind kind = ::lsp::DocumentHighlightKind::Text;
};

struct FoldingRangeData {
  TextOffset begin = 0;
  TextOffset end = 0;
  ::lsp::FoldingRangeKind kind = ::lsp::FoldingRangeKind::Region;
};

[[nodiscard]] TokenSpan token_at(std::string_view text, TextOffset offset);

[[nodiscard]] std::string
grammar_label(const grammar::AbstractElement *element);

[[nodiscard]] std::string display_type_name(std::type_index type);

[[nodiscard]] std::string location_key(const LocationData &location);
[[nodiscard]] LocationData
to_location(const workspace::AstNodeDescription &symbol);
[[nodiscard]] LocationData
to_location(const workspace::ReferenceDescription &reference);

[[nodiscard]] std::optional<LocationData>
resolve_reference_target_location(const workspace::Document &document,
                                  TextOffset offset,
                                  const services::Services &services);

[[nodiscard]] std::shared_ptr<const workspace::TextDocument>
resolve_text_document(workspace::DocumentId documentId,
                      const services::SharedServices &sharedServices);

[[nodiscard]] std::optional<::lsp::LocationLink>
to_location_link(const workspace::Document &sourceDocument,
                 TextOffset sourceOffset,
                 const LocationData &targetLocation,
                 const services::SharedServices &sharedServices);

[[nodiscard]] std::optional<::lsp::WorkspaceEdit>
to_lsp_workspace_edit(const WorkspaceEditData &workspaceEdit,
                      const services::SharedServices &sharedServices,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

[[nodiscard]] std::string
document_highlight_key(const DocumentHighlightData &highlight);

void collect_folding_ranges(const CstNodeView &node, std::string_view text,
                            std::vector<FoldingRangeData> &ranges,
                            std::unordered_set<std::string> &seen);

[[nodiscard]] bool is_link_end_char(char c) noexcept;

[[nodiscard]] ::lsp::SelectionRange
compute_selection_range(const workspace::Document &document, TextOffset offset);

[[nodiscard]] bool has_symbol_or_reference(
    std::string_view token, const workspace::IndexManager &index,
    const workspace::Documents *documents);

} // namespace pegium::lsp::detail
