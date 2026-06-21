#include <pegium/lsp/runtime/LanguageServerRuntimeState.hpp>

#include <ranges>
#include <utility>

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

void LanguageServerRuntimeState::reset() noexcept {
  utils::TransparentStringMap<std::shared_ptr<utils::CancellationTokenSource>>
      pending;
  {
    std::scoped_lock lock(_requestCancellationMutex);
    _initialized.store(false, std::memory_order_release);
    _shutdownRequested.store(false, std::memory_order_release);
    _exitRequested.store(false, std::memory_order_release);
    _initializeCapabilities = {};
    pending = std::move(_requestCancellation);
    _requestCancellation.clear();
  }
  for (const auto &source : std::views::values(pending)) {
    source->request_stop();
  }
}

bool LanguageServerRuntimeState::initialized() const noexcept {
  return _initialized.load(std::memory_order_acquire);
}

void LanguageServerRuntimeState::setInitialized(bool value) noexcept {
  _initialized.store(value, std::memory_order_release);
}

bool LanguageServerRuntimeState::shutdownRequested() const noexcept {
  return _shutdownRequested.load(std::memory_order_acquire);
}

void LanguageServerRuntimeState::setShutdownRequested(bool value) noexcept {
  _shutdownRequested.store(value, std::memory_order_release);
}

bool LanguageServerRuntimeState::exitRequested() const noexcept {
  return _exitRequested.load(std::memory_order_acquire);
}

void LanguageServerRuntimeState::setExitRequested(bool value) noexcept {
  _exitRequested.store(value, std::memory_order_release);
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

} // namespace pegium
