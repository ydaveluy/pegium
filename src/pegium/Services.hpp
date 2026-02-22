#pragma once
#include <pegium/parser/IParser.hpp>

namespace pegium {

class Services {
public:
  virtual ~Services() noexcept = default;

  virtual const parser::IParser *getParser() const = 0;
};
} // namespace pegium
