#include <pegium/utils/Cancellation.hpp>

namespace pegium::utils {

OperationCancelled::OperationCancelled()
    : std::runtime_error("Operation cancelled") {}

} // namespace pegium::utils
