#pragma once

#include <stop_token>
#include <stdexcept>

namespace pegium::utils {

using CancellationToken = std::stop_token;
using CancellationTokenSource = std::stop_source;

/// Default token used when an API accepts cancellation but the caller has none.
inline static const CancellationToken default_cancel_token{};

/// Exception thrown when cooperative cancellation is observed.
class OperationCancelled : public std::runtime_error {
public:
  OperationCancelled();
};

/// Throws `OperationCancelled` when `token` has been requested to stop.
inline void throw_if_cancelled(const CancellationToken &token) {
  if (token.stop_requested()) [[unlikely]] {
    throw OperationCancelled();
  }
}

} // namespace pegium::utils
