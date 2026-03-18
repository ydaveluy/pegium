#include <pegium/lsp/Diagnostics.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/JsonValue.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::lsp {

namespace {

constexpr int clamp_to_lsp_integer(std::int64_t value) noexcept {
  if (value < std::numeric_limits<int>::min()) {
    return std::numeric_limits<int>::min();
  }
  if (value > std::numeric_limits<int>::max()) {
    return std::numeric_limits<int>::max();
  }
  return static_cast<int>(value);
}

::lsp::DiagnosticTag to_lsp_diagnostic_tag(services::DiagnosticTag tag) {
  using enum services::DiagnosticTag;
  switch (tag) {
  case Unnecessary:
    return ::lsp::DiagnosticTag::Unnecessary;
  case Deprecated:
    return ::lsp::DiagnosticTag::Deprecated;
  }
  return ::lsp::DiagnosticTag::Unnecessary;
}

} // namespace

void publish_diagnostics(
    ::lsp::MessageHandler *messageHandler,
    const workspace::DocumentDiagnosticsSnapshot &snapshot) {
  if (messageHandler == nullptr) {
    return;
  }

  ::lsp::notifications::TextDocument_PublishDiagnostics::Params params{};
  params.uri = ::lsp::Uri::parse(snapshot.uri);
  if (snapshot.version.has_value()) {
    // The protocol only carries 32-bit integers for versions.
    params.version = clamp_to_lsp_integer(*snapshot.version);
  }
  params.diagnostics.reserve(snapshot.diagnostics.size());
  workspace::Document positionDocument;
  positionDocument.setText(snapshot.text);

  for (const auto &diagnostic : snapshot.diagnostics) {
    ::lsp::Diagnostic lspDiagnostic{};
    lspDiagnostic.severity = to_lsp_diagnostic_severity(diagnostic.severity);
    lspDiagnostic.source = diagnostic.source;
    lspDiagnostic.message = diagnostic.message;
    if (diagnostic.code.has_value()) {
      if (const auto *code = std::get_if<std::int64_t>(&*diagnostic.code)) {
        lspDiagnostic.code = ::lsp::OneOf<int, ::lsp::String>(
            clamp_to_lsp_integer(*code));
      } else if (const auto *code = std::get_if<std::string>(&*diagnostic.code)) {
        lspDiagnostic.code = ::lsp::OneOf<int, ::lsp::String>(*code);
      }
    }
    if (diagnostic.codeDescription.has_value()) {
      const auto uri = ::lsp::Uri::parse(*diagnostic.codeDescription);
      if (uri.isValid()) {
        ::lsp::CodeDescription codeDescription{};
        codeDescription.href = uri;
        lspDiagnostic.codeDescription = std::move(codeDescription);
      }
    }
    if (!diagnostic.tags.empty()) {
      ::lsp::Array<::lsp::DiagnosticTagEnum> tags;
      tags.reserve(diagnostic.tags.size());
      for (const auto tag : diagnostic.tags) {
        tags.push_back(to_lsp_diagnostic_tag(tag));
      }
      lspDiagnostic.tags = std::move(tags);
    }
    if (!diagnostic.relatedInformation.empty()) {
      ::lsp::Array<::lsp::DiagnosticRelatedInformation> relatedInformation;
      relatedInformation.reserve(diagnostic.relatedInformation.size());
      for (const auto &entry : diagnostic.relatedInformation) {
        ::lsp::DiagnosticRelatedInformation related{};
        const auto entryUri =
            entry.uri.empty() ? ::lsp::Uri::parse(snapshot.uri)
                              : ::lsp::Uri::parse(entry.uri);
        related.location.uri =
            entryUri.isValid() ? entryUri : ::lsp::Uri::parse(snapshot.uri);
        related.location.range.start = positionDocument.offsetToPosition(entry.begin);
        related.location.range.end = positionDocument.offsetToPosition(
            entry.end >= entry.begin ? entry.end : entry.begin);
        related.message = entry.message;
        relatedInformation.push_back(std::move(related));
      }
      lspDiagnostic.relatedInformation = std::move(relatedInformation);
    }
    if (diagnostic.data.has_value()) {
      lspDiagnostic.data = to_lsp_any(*diagnostic.data);
    }

    lspDiagnostic.range.start = positionDocument.offsetToPosition(diagnostic.begin);
    lspDiagnostic.range.end = positionDocument.offsetToPosition(
        diagnostic.end >= diagnostic.begin ? diagnostic.end : diagnostic.begin);

    params.diagnostics.push_back(std::move(lspDiagnostic));
  }

  messageHandler
      ->sendNotification<::lsp::notifications::TextDocument_PublishDiagnostics>(
          std::move(params));
}

} // namespace pegium::lsp
