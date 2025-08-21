#pragma once

#include "FDBConfig.h"
#include "CustomKvEngine.h"

namespace hf3fs::kv {
struct HybridKvEngineConfig : public ConfigBase<HybridKvEngineConfig> {
  bool operator==(const HybridKvEngineConfig &other) const { return static_cast<const ConfigBase &>(*this) == other; }

  CONFIG_ITEM(use_memkv, false);
  CONFIG_OBJ(fdb, kv::fdb::FDBConfig);
  CONFIG_OBJ(custom_kv, CustomKvEngineConfig);
  CONFIG_ITEM(kv_engine_type, std::string("fdb")); // "fdb", "memkv", or "custom"
};
}  // namespace hf3fs::kv
