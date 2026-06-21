#pragma once

#include <functional>
#include <future>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::workspace {

/// Read/write coordination primitive for workspace operations.
class WorkspaceLock {
public:
  // Downgrades the in-progress exclusive write to a shared read: subsequent and
  // pending reads may run while the write action continues (e.g. validation).
  // Idempotent; the lock also calls it once the action returns.
  using Downgrade = std::function<void()>;
  using WriteAction = std::function<void(const utils::CancellationToken &cancelToken,
                                         const Downgrade &downgrade)>;
  using ReadAction = std::function<void()>;

  virtual ~WorkspaceLock() noexcept = default;
  // Writes run exclusively. A newly queued write has priority over reads that
  // are still waiting to run.
  //
  // The action receives a cancellation token that is cancelled when a newer
  // write supersedes it or cancelWrite() is requested, and a downgrade callback
  // it may invoke to release exclusivity for the remainder of its work so reads
  // can proceed. This applies equally to bootstrap writes such as the initial
  // workspace build and to later document update writes.
  [[nodiscard]] virtual std::future<void> write(WriteAction action) = 0;
  // Reads wait for pending writes and may then execute in parallel with other
  // reads.
  [[nodiscard]] virtual std::future<void> read(ReadAction action) = 0;
  // Cancels the latest queued or running write that is still relevant.
  virtual void cancelWrite() = 0;
};

} // namespace pegium::workspace
