#pragma once

#include <vector>

#include <pegium/core/services/DefaultCoreService.hpp>
#include <pegium/core/validation/DocumentValidator.hpp>

namespace pegium::services {
struct CoreServices;
}

namespace pegium::validation {

/// Default validator combining parser, linker, and custom validation checks.
class DefaultDocumentValidator : public DocumentValidator,
                                 protected services::DefaultCoreService {
public:
  using services::DefaultCoreService::DefaultCoreService;

  [[nodiscard]] std::vector<services::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const ValidationOptions &options,
                   const utils::CancellationToken &cancelToken) const override;

private:
  [[nodiscard]] bool run_builtin_validation(
      const ValidationOptions &options) const noexcept;
  [[nodiscard]] bool run_custom_validation(
      const ValidationOptions &options) const noexcept;
  void processParsingErrors(const workspace::Document &document,
                            std::vector<services::Diagnostic> &diagnostics,
                            const utils::CancellationToken &cancelToken) const;
  void processLinkingErrors(const workspace::Document &document,
                            std::vector<services::Diagnostic> &diagnostics,
                            const std::string &source,
                            const utils::CancellationToken &cancelToken) const;
  void validateAst(const AstNode &rootNode,
                   std::vector<services::Diagnostic> &diagnostics,
                   const ValidationOptions &options, const std::string &source,
                   const utils::CancellationToken &cancelToken) const;
  void validateAstBefore(const AstNode &rootNode,
                         const ValidationAcceptor &acceptor,
                         std::span<const std::string> categories,
                         const utils::CancellationToken &cancelToken) const;
  void validateAstNodes(const AstNode &rootNode,
                        const ValidationAcceptor &acceptor,
                        std::span<const std::string> categories,
                        const utils::CancellationToken &cancelToken) const;
  void validateAstAfter(const AstNode &rootNode,
                        const ValidationAcceptor &acceptor,
                        std::span<const std::string> categories,
                        const utils::CancellationToken &cancelToken) const;
};

} // namespace pegium::validation
