#pragma once

#include <stdexcept>
#include <string>

namespace pegium::utils {

/// Base exception type used for Pegium-specific failures.
class PegiumError : public std::runtime_error {
public:
  explicit PegiumError(const std::string &message);
};

} // namespace pegium::utils
