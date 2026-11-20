#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/version.h"
#include <string>

namespace esphome {
namespace elero {
namespace compat {

// ESPHome 2026.11 moves entity names to flash-aware accessors. Keep older
// supported releases working without using the deprecated API on new ones.
inline std::string entity_name(const EntityBase &entity) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  char buffer[ENTITY_NAME_BUF_SIZE];
  const auto name = entity.get_name_to(buffer);
  return std::string(name.c_str(), name.size());
#else
  return std::string(entity.get_name().c_str());
#endif
}

// Pass through LOG_STR_ARG at the logging call site; no heap copy is needed.
inline auto entity_log_name(const EntityBase &entity) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  return entity.get_log_name();
#else
  return entity.get_name().c_str();
#endif
}

}  // namespace compat
}  // namespace elero
}  // namespace esphome
