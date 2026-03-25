#pragma once

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium::fuzz::adversarial {

bool register_adversarial_language_services(
    pegium::SharedServices &sharedServices);

} // namespace pegium::fuzz::adversarial
