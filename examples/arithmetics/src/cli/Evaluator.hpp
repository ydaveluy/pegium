#pragma once

#include <string_view>

#include <pegium/core/services/CoreServices.hpp>

namespace arithmetics::cli {

int eval_file(std::string_view fileName,
              const pegium::CoreServices &services);

} // namespace arithmetics::cli
