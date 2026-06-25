#pragma once

#include <pegium/core/services/CoreServices.hpp>

namespace pegium {

/// Convenience base class for default services bound to one `CoreServices`.
///
/// Sibling-access discipline: reach the per-container sibling services
/// (`services.references.*`, `services.validation.*`, `services.workspace.*`,
/// `services.documentation.*`, `services.parser`) through `services` on *each
/// call* — do NOT cache a sibling pointer or reference at construction time.
/// `installDefaultCoreServices(...)` fills empty slots, and a language module
/// then replaces some of them, so a sibling captured in a constructor may point
/// at a default that was already discarded. Capturing the stable
/// `services.shared` reference at construction is fine: it is fixed when the
/// `CoreServices` is constructed and never replaced.
class DefaultCoreService {
public:
  explicit DefaultCoreService(const CoreServices &services) noexcept
      : services(services) {}
  virtual ~DefaultCoreService() noexcept = default;

protected:
  const CoreServices &services;
};

} // namespace pegium
