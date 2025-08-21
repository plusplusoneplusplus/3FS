#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/kv/IKVEngine.h"
#include "common/utils/ConfigBase.h"

namespace hf3fs::kv {

// Configuration for Custom KV Engine
struct CustomKvEngineConfig : public ConfigBase<CustomKvEngineConfig> {
  CONFIG_ITEM(cluster_endpoints, std::vector<std::string>{});
  CONFIG_ITEM(transaction_timeout_ms, 10000);
  CONFIG_ITEM(max_retry_count, 10);
  CONFIG_ITEM(connection_pool_size, 10);
  CONFIG_ITEM(connection_timeout_ms, 5000);
  CONFIG_ITEM(read_timeout_ms, 30000);
  CONFIG_ITEM(write_timeout_ms, 30000);
};

// Forward declarations
class CustomTransaction;
class CustomReadOnlyTransaction;
class CustomKvEngineImpl;

// Custom KV Engine implementation that connects to the Rust transactional KV server
class CustomKvEngine : public IKVEngine {
 public:
  explicit CustomKvEngine(const CustomKvEngineConfig& config);
  ~CustomKvEngine();

  // IKVEngine interface
  std::unique_ptr<IReadOnlyTransaction> createReadonlyTransaction() override;
  std::unique_ptr<IReadWriteTransaction> createReadWriteTransaction() override;

  // Health check and connection management
  bool isHealthy() const;
  void reconnect();
  
  // Access to client handle for transactions
  void* getClientHandle() const;

 private:
  std::shared_ptr<CustomKvEngineImpl> impl_;
};

}  // namespace hf3fs::kv