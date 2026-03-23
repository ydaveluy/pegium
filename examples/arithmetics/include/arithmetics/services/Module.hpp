#pragma once

#include <memory>

#include <arithmetics/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace arithmetics::services {

std::unique_ptr<ArithmeticsServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId = "arithmetics");

bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace arithmetics::services
