#pragma once

#include <stop_token>
#include <stdexcept>

namespace pegium::utils {

using CancellationToken = std::stop_token;
using CancellationTokenSource = std::stop_source;

inline static const CancellationToken default_cancel_token{};

class OperationCancelled : public std::runtime_error {
public:
  OperationCancelled();
};

inline void throw_if_cancelled(const CancellationToken &token) {
  if (token.stop_requested()) [[unlikely]] {
    throw OperationCancelled();
  }
}

} // namespace pegium::utils
