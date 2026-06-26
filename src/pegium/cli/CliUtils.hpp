#pragma once

#include <iosfwd>
#include <memory>
#include <string_view>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace pegium::workspace {
struct Document;
}

namespace pegium {

/// Creates shared services for standalone CLI execution.
///
/// Returns an owning handle (SharedCoreServices is non-movable: its installed
/// services hold back-references to it, so it must stay at a fixed address).
[[nodiscard]] std::unique_ptr<pegium::SharedCoreServices> make_shared_services();

/// Loads or rebuilds the document for `path` and returns the shared snapshot.
[[nodiscard]] std::shared_ptr<workspace::Document>
build_document_from_path(std::string_view path,
                         const pegium::CoreServices &services,
                         bool validation = true);

/// Returns whether `document` currently contains error diagnostics.
[[nodiscard]] bool
has_error_diagnostics(const workspace::Document &document) noexcept;

/// Prints error diagnostics from `document` to `out`.
void print_error_diagnostics(const workspace::Document &document,
                             std::ostream &out);

} // namespace pegium
