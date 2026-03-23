#pragma once

#include <memory>
#include <string>

#include <statemachine/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace statemachine::services {

std::unique_ptr<StatemachineServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId = "statemachine");

bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace statemachine::services
