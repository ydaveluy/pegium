#pragma once

#include <pegium/references/DefaultScopeProvider.hpp>

namespace arithmetics::services::references {

class ArithmeticsScopeProvider final
    : public pegium::references::DefaultScopeProvider {
public:
  using pegium::references::DefaultScopeProvider::DefaultScopeProvider;
};

} // namespace arithmetics::services::references
