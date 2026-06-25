#pragma once

#include <memory>

#include <requirements/core/validation/RequirementsValidator.hpp>
#include <requirements/core/validation/TestsValidator.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace requirements {

/// Requirements-language-specific services grafted onto any pegium container.
struct RequirementsAddedServices {
  std::unique_ptr<validation::RequirementsValidator> validator;
};

/// Tests-language-specific services grafted onto any pegium container.
struct TestsAddedServices {
  std::unique_ptr<validation::TestsValidator> validator;
};

/// Core-only requirements-language services.
struct RequirementsCoreServices final : pegium::CoreServices,
                                        RequirementsAddedServices {
  using pegium::CoreServices::CoreServices;
};

/// Core-only tests-language services.
struct TestsCoreServices final : pegium::CoreServices, TestsAddedServices {
  using pegium::CoreServices::CoreServices;
};

} // namespace requirements
