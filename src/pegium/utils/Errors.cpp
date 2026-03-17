#include <pegium/utils/Errors.hpp>
namespace pegium::utils {

PegiumError::PegiumError(const std::string &message) : std::runtime_error(message) {}

} // namespace pegium::utils
