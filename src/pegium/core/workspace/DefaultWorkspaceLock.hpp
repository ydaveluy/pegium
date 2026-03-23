#pragma once

#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <pegium/core/workspace/WorkspaceLock.hpp>

namespace pegium::workspace {

/// Default workspace lock serializing writes and batching concurrent reads.
class DefaultWorkspaceLock : public WorkspaceLock {
public:
  DefaultWorkspaceLock();
  ~DefaultWorkspaceLock() override;

  [[nodiscard]] std::future<void> write(WriteAction action) override;
  [[nodiscard]] std::future<void> read(ReadAction action) override;
  void cancelWrite() override;

private:
  struct QueueEntry {
    bool isWrite = false;
    WriteAction writeAction;
    ReadAction readAction;
    utils::CancellationTokenSource cancellation;
    std::promise<void> completion;
  };

  void run(std::stop_token stopToken);

  mutable std::mutex _queueMutex;
  std::condition_variable _queueCv;
  std::deque<QueueEntry> _writeQueue;
  std::deque<QueueEntry> _readQueue;
  utils::CancellationTokenSource _previousWriteCancellation;
  bool _stopping = false;
  std::jthread _worker;

  mutable std::shared_mutex _mutex;
};

} // namespace pegium::workspace
