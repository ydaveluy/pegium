#pragma once

#include <stdexcept>
#include <string>

namespace pegium::utils {

class PegiumError : public std::runtime_error {
public:
  explicit PegiumError(const std::string &message);
};

} // namespace pegium::utils
