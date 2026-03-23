#pragma once

#include <iosfwd>
#include <memory>
#include <string_view>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium::workspace {
struct Document;
}

namespace pegium::cli {

/// Creates shared services for standalone CLI execution.
[[nodiscard]] SharedServices make_shared_services();

/// Loads or rebuilds the document for `path` and returns the shared snapshot.
[[nodiscard]] std::shared_ptr<workspace::Document>
build_document_from_path(std::string_view path,
                         const services::CoreServices &services,
                         bool validation = true);

/// Returns whether `document` currently contains error diagnostics.
[[nodiscard]] bool
has_error_diagnostics(const workspace::Document &document) noexcept;

/// Prints error diagnostics from `document` to `out`.
void print_error_diagnostics(const workspace::Document &document,
                             std::ostream &out);

} // namespace pegium::cli
