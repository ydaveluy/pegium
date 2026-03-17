#include <pegium/lsp/AbstractSignatureHelpProvider.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>

namespace pegium::lsp {

::lsp::SignatureHelpOptions
AbstractSignatureHelpProvider::signatureHelpOptions() const {
  ::lsp::SignatureHelpOptions options{};
  options.triggerCharacters = ::lsp::Array<::lsp::String>{"("};
  options.retriggerCharacters = ::lsp::Array<::lsp::String>{","};
  return options;
}

std::optional<::lsp::SignatureHelp>
AbstractSignatureHelpProvider::provideSignatureHelp(
    const workspace::Document &document, const ::lsp::SignatureHelpParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  if (document.parseResult.value == nullptr) {
    return std::nullopt;
  }
  const auto offset = document.positionToOffset(params.position);
  const auto *element =
      pegium::find_ast_node_at_offset(*document.parseResult.value, offset);
  if (element == nullptr) {
    return std::nullopt;
  }
  return getSignatureFromElement(*element, cancelToken);
}

} // namespace pegium::lsp
