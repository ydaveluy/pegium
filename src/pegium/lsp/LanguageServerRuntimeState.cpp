#include <pegium/lsp/LanguageServerRuntimeState.hpp>

#include <ranges>
#include <utility>

#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

void LanguageServerRuntimeState::reset() noexcept {
  utils::TransparentStringMap<std::shared_ptr<utils::CancellationTokenSource>>
      pending;
  {
    std::scoped_lock lock(_requestCancellationMutex);
    _initialized = false;
    _shutdownRequested = false;
    _exitRequested = false;
    _initializeCapabilities = {};
    pending = std::move(_requestCancellation);
    _requestCancellation.clear();
  }
  for (const auto &source : std::views::values(pending)) {
    if (source != nullptr) {
      source->request_stop();
    }
  }
}

bool LanguageServerRuntimeState::initialized() const noexcept {
  return _initialized;
}

void LanguageServerRuntimeState::setInitialized(bool value) noexcept {
  _initialized = value;
}

bool LanguageServerRuntimeState::shutdownRequested() const noexcept {
  return _shutdownRequested;
}

void LanguageServerRuntimeState::setShutdownRequested(bool value) noexcept {
  _shutdownRequested = value;
}

bool LanguageServerRuntimeState::exitRequested() const noexcept {
  return _exitRequested;
}

void LanguageServerRuntimeState::setExitRequested(bool value) noexcept {
  _exitRequested = value;
}

const workspace::InitializeCapabilities &
LanguageServerRuntimeState::initializeCapabilities() const noexcept {
  return _initializeCapabilities;
}

void LanguageServerRuntimeState::setInitializeCapabilities(
    workspace::InitializeCapabilities capabilities) noexcept {
  _initializeCapabilities = std::move(capabilities);
}

void LanguageServerRuntimeState::registerRequestCancellation(
    std::string requestKey,
    std::shared_ptr<utils::CancellationTokenSource> source) {
  if (requestKey.empty() || source == nullptr) {
    return;
  }
  std::scoped_lock lock(_requestCancellationMutex);
  _requestCancellation.insert_or_assign(std::move(requestKey), std::move(source));
}

void LanguageServerRuntimeState::clearRequestCancellation(
    std::string_view requestKey,
    const std::shared_ptr<utils::CancellationTokenSource> &source) {
  if (requestKey.empty()) {
    return;
  }
  std::scoped_lock lock(_requestCancellationMutex);
  const auto it = _requestCancellation.find(requestKey);
  if (it == _requestCancellation.end()) {
    return;
  }
  if (!source || it->second == source) {
    _requestCancellation.erase(it);
    _requestCancellationCv.notify_all();
  }
}

void LanguageServerRuntimeState::cancelRequestByKey(std::string_view requestKey) {
  if (requestKey.empty()) {
    return;
  }
  std::shared_ptr<utils::CancellationTokenSource> source;
  {
    std::scoped_lock lock(_requestCancellationMutex);
    const auto it = _requestCancellation.find(requestKey);
    if (it != _requestCancellation.end()) {
      source = it->second;
    }
  }
  if (source != nullptr) {
    source->request_stop();
  }
}

void LanguageServerRuntimeState::waitForPendingRequests() {
  std::unique_lock lock(_requestCancellationMutex);
  _requestCancellationCv.wait(
      lock, [this]() { return _requestCancellation.empty(); });
}

} // namespace pegium::lsp
