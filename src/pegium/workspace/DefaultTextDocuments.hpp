#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/utils/Event.hpp>
#include <pegium/workspace/TextDocuments.hpp>

namespace pegium::workspace {

class DefaultTextDocuments : public TextDocuments {
public:
  [[nodiscard]] std::shared_ptr<const TextDocument>
  open(std::string uri, std::string languageId, std::string text,
       std::optional<std::int64_t> clientVersion = std::nullopt) override;

  [[nodiscard]] std::shared_ptr<const TextDocument>
  replaceText(std::string_view uri, std::string text, std::string languageId = {},
              std::optional<std::int64_t> clientVersion = std::nullopt) override;

  [[nodiscard]] std::shared_ptr<const TextDocument> applyContentChanges(
      std::string_view uri, std::span<const TextDocumentContentChange> changes,
      std::optional<std::int64_t> clientVersion = std::nullopt) override;

  [[nodiscard]] std::shared_ptr<const TextDocument>
  save(std::string_view uri, std::optional<std::string> text = std::nullopt) override;

  bool close(std::string_view uri) override;

  [[nodiscard]] std::shared_ptr<const TextDocument>
  get(std::string_view uri) const override;

  utils::ScopedDisposable
  onDidOpen(std::function<void(const TextDocumentEvent &)> listener) override;
  utils::ScopedDisposable onDidChangeContent(
      std::function<void(const TextDocumentEvent &)> listener) override;
  utils::ScopedDisposable
  onDidSave(std::function<void(const TextDocumentEvent &)> listener) override;
  utils::ScopedDisposable
  onDidClose(std::function<void(const TextDocumentEvent &)> listener) override;
  utils::ScopedDisposable onWillSave(
      std::function<void(const TextDocumentWillSaveEvent &)> listener) override;
  utils::ScopedDisposable onWillSaveWaitUntil(
      std::function<std::vector<TextDocumentEdit>(
          const TextDocumentWillSaveEvent &)>
          listener) override;

  bool willSave(std::string_view uri, TextDocumentSaveReason reason) override;
  [[nodiscard]] std::vector<TextDocumentEdit>
  willSaveWaitUntil(std::string_view uri,
                    TextDocumentSaveReason reason) override;

  void clear() override;

private:
  void emitDidOpen(const std::shared_ptr<const TextDocument> &document);
  void emitDidChangeContent(const std::shared_ptr<const TextDocument> &document);
  void emitDidSave(const std::shared_ptr<const TextDocument> &document);
  void emitDidClose(const std::shared_ptr<const TextDocument> &document);
  void emitWillSave(const TextDocumentWillSaveEvent &event);
  [[nodiscard]] std::vector<TextDocumentEdit>
  emitWillSaveWaitUntil(const TextDocumentWillSaveEvent &event) const;

  mutable std::mutex _mutex;
  std::unordered_map<std::string, std::shared_ptr<const TextDocument>> _documents;
  utils::EventEmitter<TextDocumentEvent> _onDidOpen;
  utils::EventEmitter<TextDocumentEvent> _onDidChangeContent;
  utils::EventEmitter<TextDocumentEvent> _onDidSave;
  utils::EventEmitter<TextDocumentEvent> _onDidClose;
  utils::EventEmitter<TextDocumentWillSaveEvent> _onWillSave;
  std::unordered_map<
      std::size_t,
      std::function<std::vector<TextDocumentEdit>(const TextDocumentWillSaveEvent &)>>
      _onWillSaveWaitUntil;
  std::size_t _nextWillSaveWaitUntilId = 0;
};

} // namespace pegium::workspace
