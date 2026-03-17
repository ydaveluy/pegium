#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/utils/Disposable.hpp>
#include <pegium/workspace/TextDocument.hpp>

namespace pegium::workspace {

struct TextDocumentEvent {
  std::shared_ptr<const TextDocument> document;
};

enum class TextDocumentSaveReason {
  Manual,
  AfterDelay,
  FocusOut,
};

struct TextDocumentWillSaveEvent {
  std::shared_ptr<const TextDocument> document;
  TextDocumentSaveReason reason = TextDocumentSaveReason::Manual;
};

struct TextDocumentEdit {
  TextDocumentContentChangeRange range;
  std::string newText;
};

class TextDocuments {
public:
  virtual ~TextDocuments() noexcept = default;

  [[nodiscard]] virtual std::shared_ptr<const TextDocument>
  open(std::string uri, std::string languageId, std::string text,
       std::optional<std::int64_t> clientVersion = std::nullopt) = 0;

  [[nodiscard]] virtual std::shared_ptr<const TextDocument>
  replaceText(std::string_view uri, std::string text, std::string languageId = {},
              std::optional<std::int64_t> clientVersion = std::nullopt) = 0;

  [[nodiscard]] virtual std::shared_ptr<const TextDocument> applyContentChanges(
      std::string_view uri, std::span<const TextDocumentContentChange> changes,
      std::optional<std::int64_t> clientVersion = std::nullopt) = 0;

  [[nodiscard]] virtual std::shared_ptr<const TextDocument>
  save(std::string_view uri, std::optional<std::string> text = std::nullopt) = 0;

  virtual bool close(std::string_view uri) = 0;

  [[nodiscard]] virtual std::shared_ptr<const TextDocument>
  get(std::string_view uri) const = 0;

  virtual utils::ScopedDisposable onDidOpen(
      std::function<void(const TextDocumentEvent &)> listener) = 0;
  virtual utils::ScopedDisposable onDidChangeContent(
      std::function<void(const TextDocumentEvent &)> listener) = 0;
  virtual utils::ScopedDisposable onDidSave(
      std::function<void(const TextDocumentEvent &)> listener) = 0;
  virtual utils::ScopedDisposable onDidClose(
      std::function<void(const TextDocumentEvent &)> listener) = 0;
  virtual utils::ScopedDisposable onWillSave(
      std::function<void(const TextDocumentWillSaveEvent &)> listener) = 0;
  virtual utils::ScopedDisposable onWillSaveWaitUntil(
      std::function<std::vector<TextDocumentEdit>(
          const TextDocumentWillSaveEvent &)>
          listener) = 0;

  virtual bool willSave(std::string_view uri, TextDocumentSaveReason reason) = 0;
  [[nodiscard]] virtual std::vector<TextDocumentEdit>
  willSaveWaitUntil(std::string_view uri, TextDocumentSaveReason reason) = 0;

  virtual void clear() = 0;
};

} // namespace pegium::workspace
