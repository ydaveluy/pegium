#include <pegium/core/workspace/DefaultWorkspaceLock.hpp>

#include <exception>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::workspace {

DefaultWorkspaceLock::DefaultWorkspaceLock()
    : _worker([this](std::stop_token stopToken) { run(stopToken); }) {}

DefaultWorkspaceLock::~DefaultWorkspaceLock() {
  {
    std::scoped_lock lock(_queueMutex);
    _stopping = true;
    for (auto &entry : _writeQueue) {
      entry.cancellation.request_stop();
    }
  }
  this->cancelWrite();
  _queueCv.notify_all();
}

std::future<void> DefaultWorkspaceLock::write(WriteAction action) {
  if (!action) {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  QueueEntry entry;
  entry.isWrite = true;
  entry.writeAction = std::move(action);
  auto future = entry.completion.get_future();

  {
    std::scoped_lock lock(_queueMutex);
    if (_stopping) {
      std::promise<void> promise;
      promise.set_value();
      return promise.get_future();
    }
    _previousWriteCancellation.request_stop();
    _previousWriteCancellation = entry.cancellation;
    _writeQueue.push_back(std::move(entry));
  }
  _queueCv.notify_all();
  return future;
}

std::future<void> DefaultWorkspaceLock::read(ReadAction action) {
  if (!action) {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  QueueEntry entry;
  entry.isWrite = false;
  entry.readAction = std::move(action);
  auto future = entry.completion.get_future();

  {
    std::scoped_lock lock(_queueMutex);
    if (_stopping) {
      std::promise<void> promise;
      promise.set_value();
      return promise.get_future();
    }
    _readQueue.push_back(std::move(entry));
  }
  _queueCv.notify_all();
  return future;
}

void DefaultWorkspaceLock::cancelWrite() {
  std::scoped_lock lock(_queueMutex);
  _previousWriteCancellation.request_stop();
}

void DefaultWorkspaceLock::run(std::stop_token stopToken) {
  while (true) {
    std::vector<QueueEntry> entries;
    {
      std::unique_lock lock(_queueMutex);
      _queueCv.wait(lock, [this, &stopToken]() {
        return _stopping || stopToken.stop_requested() || !_writeQueue.empty() ||
               !_readQueue.empty();
      });

      if ((_stopping || stopToken.stop_requested()) && _writeQueue.empty() &&
          _readQueue.empty()) {
        return;
      }

      if (!_writeQueue.empty()) {
        entries.push_back(std::move(_writeQueue.front()));
        _writeQueue.pop_front();
      } else {
        while (!_readQueue.empty()) {
          entries.push_back(std::move(_readQueue.front()));
          _readQueue.pop_front();
        }
      }
    }

    std::vector<std::jthread> readWorkers;
    readWorkers.reserve(entries.size());
    for (auto &entry : entries) {
      const bool shuttingDown = _stopping || stopToken.stop_requested();
      if (shuttingDown) {
        entry.cancellation.request_stop();
      }

      if (entry.isWrite) {
        try {
          if (!shuttingDown) {
            std::unique_lock lock(_mutex);
            entry.writeAction(entry.cancellation.get_token());
          }
          entry.completion.set_value();
        } catch (const utils::OperationCancelled &) {
          entry.completion.set_value();
        } catch (const std::exception &) {
          entry.completion.set_exception(std::current_exception());
        }
      } else {
        auto *entryPtr = &entry;
        readWorkers.emplace_back([this, entryPtr]() {
          try {
            std::shared_lock lock(_mutex);
            entryPtr->readAction();
            entryPtr->completion.set_value();
          } catch (const std::exception &) {
            entryPtr->completion.set_exception(std::current_exception());
          }
        });
      }
    }
  }
}

} // namespace pegium::workspace
