#pragma once
#include <pegium/IParser.hpp>

namespace pegium {


class Services {
public:
  virtual ~Services() noexcept = default;

  virtual const IParser* getParser() const = 0;


};
} // namespace pegium