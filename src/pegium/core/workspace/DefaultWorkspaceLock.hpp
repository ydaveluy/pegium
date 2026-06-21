#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

#include <pegium/core/workspace/WorkspaceLock.hpp>

namespace pegium::workspace {

/// Default workspace lock. Writes run exclusively and serially on a background
/// worker (a newer write supersedes the previous one); reads run concurrently
/// in the calling thread. A write may downgrade to a shared read so its tail
/// (validation) runs alongside reads.
///
/// Destruction precondition: no thread may be inside, or about to call, write()
/// / read() / cancelWrite() when the lock is destroyed. The destructor drains
/// reads that have already entered the gate, but it cannot protect a thread that
/// has called a method and not yet acquired the internal mutex. Owners must
/// quiesce their request handlers before destroying the lock.
class DefaultWorkspaceLock : public WorkspaceLock {
public:
  DefaultWorkspaceLock();
  ~DefaultWorkspaceLock() override;

  [[nodiscard]] std::future<void> write(WriteAction action) override;
  [[nodiscard]] std::future<void> read(ReadAction action) override;
  void cancelWrite() override;

private:
  struct WriteEntry {
    WriteAction action;
    utils::CancellationTokenSource cancellation;
    std::promise<void> completion;
  };

  void run(std::stop_token stopToken);

  // Write queue + supersession.
  mutable std::mutex _queueMutex;
  std::condition_variable _queueCv;
  std::deque<WriteEntry> _writeQueue;
  utils::CancellationTokenSource _previousWriteCancellation;
  std::atomic_bool _stopping{false};

  // Read/write gate. The exclusive write→shared read downgrade flips _writeHeld
  // to a reader under _gateMutex with no window for a new writer. _writePending
  // counts writes that are queued or holding exclusivity (but not yet
  // downgraded), so reads stay blocked until every pending write has drained.
  mutable std::mutex _gateMutex;
  std::condition_variable _gateCv;
  bool _writeHeld = false;
  std::size_t _readers = 0;
  std::size_t _writePending = 0;
  // Threads that have entered read()'s gate section — i.e. already acquired
  // _gateMutex and incremented this — whether blocked or running their action.
  // The destructor drains this so it never destroys the gate primitives under a
  // reader that has already entered the gate. It does NOT cover a thread that
  // has called read() but not yet acquired _gateMutex (see the class precondition).
  std::size_t _activeReads = 0;

  std::jthread _worker;
};

} // namespace pegium::workspace
