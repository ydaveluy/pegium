#pragma once

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Default folding-range provider based on the CST structure.
class DefaultFoldingRangeProvider : protected DefaultLanguageService,
                                public ::pegium::FoldingRangeProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::FoldingRange>
  getFoldingRanges(const workspace::Document &document,
                   const ::lsp::FoldingRangeParams &params,
                   const utils::CancellationToken &cancelToken) const override;

protected:
  /// Returns whether the last line of a folding range stays folded.
  ///
  /// By default a comment range and a range whose last character closes a
  /// bracket (`}`, `)`, `]`) keep that closing line visible, so only their
  /// interior folds. Override to customize this per language.
  [[nodiscard]] virtual bool
  includeLastFoldingLine(const ::lsp::FoldingRangeKindEnum &kind,
                         char lastCharacter) const;
};

} // namespace pegium
