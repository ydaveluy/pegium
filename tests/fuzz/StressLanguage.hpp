#pragma once

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium::fuzz::stress {

bool register_stress_language_services(
    pegium::SharedServices &sharedServices);

} // namespace pegium::fuzz::stress
