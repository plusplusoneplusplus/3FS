#include "CustomKvEngine.h"

#include <folly/logging/xlog.h>
#include <memory>
#include <string>
#include <thread>

#include "CustomTransaction.h"
#include "common/utils/Status.h"

extern "C" {
#include "third_party/kv/rust/client/include/kvstore_client.h"
}

namespace hf3fs::kv {

// Implementation class for CustomKvEngine
class CustomKvEngineImpl {
 public:
  explicit CustomKvEngineImpl(const CustomKvEngineConfig& config)
      : config_(config),
        healthy_(false) {
    initialize();
  }

  ~CustomKvEngineImpl() {
    cleanup();
  }

  bool isHealthy() const {
    return healthy_;
  }

  void reconnect() {
    cleanup();
    initialize();
  }
  
  void* getClientHandle() const {
    return client_handle_;
  }

  std::unique_ptr<IReadOnlyTransaction> createReadonlyTransaction() {
    if (!healthy_) {
      XLOG(ERR) << "CustomKvEngine is not healthy, cannot create readonly transaction";
      return nullptr;
    }

    try {
      // Create a new readonly transaction with the Rust client
      auto transaction_id = generateTransactionId();
      return std::make_unique<CustomReadOnlyTransaction>(transaction_id, shared_from_this(), client_handle_);
    } catch (const std::exception& e) {
      XLOG(ERR) << "Failed to create readonly transaction: " << e.what();
      return nullptr;
    }
  }

  std::unique_ptr<IReadWriteTransaction> createReadWriteTransaction() {
    if (!healthy_) {
      XLOG(ERR) << "CustomKvEngine is not healthy, cannot create read-write transaction";
      return nullptr;
    }

    try {
      // Create a new read-write transaction with the Rust client
      auto transaction_id = generateTransactionId();
      return std::make_unique<CustomTransaction>(transaction_id, shared_from_this(), client_handle_);
    } catch (const std::exception& e) {
      XLOG(ERR) << "Failed to create read-write transaction: " << e.what();
      return nullptr;
    }
  }

  std::shared_ptr<CustomKvEngineImpl> shared_from_this() {
    return shared_from_this_;
  }

  void setSharedThis(std::shared_ptr<CustomKvEngineImpl> ptr) {
    shared_from_this_ = ptr;
  }

 private:
  void initialize() {
    try {
      // Initialize connection to Rust KV server
      XLOG(INFO) << "Initializing CustomKvEngine with endpoints: ";
      for (const auto& endpoint : config_.cluster_endpoints()) {
        XLOG(INFO) << "  " << endpoint;
      }
      
      // Initialize the KV library
      int init_result = kv_init();
      if (init_result != 0) {
        XLOG(ERR) << "Failed to initialize KV library";
        healthy_ = false;
        return;
      }
      
      // Create client handle to the first endpoint (simple for now)
      if (!config_.cluster_endpoints().empty()) {
        std::string primary_endpoint = config_.cluster_endpoints()[0];
        client_handle_ = kv_client_create(primary_endpoint.c_str());
        
        if (!client_handle_) {
          XLOG(ERR) << "Failed to create KV client for endpoint: " << primary_endpoint;
          healthy_ = false;
          return;
        }
        
        // Test connection with ping
        const char* ping_msg = "hello";
        KvFutureHandle ping_future = kv_client_ping(client_handle_, 
                                                   reinterpret_cast<const uint8_t*>(ping_msg), 
                                                   static_cast<int>(strlen(ping_msg)));
        if (ping_future) {
          // For now, don't wait for ping response in initialization
          // In production, you might want to wait for ping to complete
          healthy_ = true;
          XLOG(INFO) << "CustomKvEngine initialized successfully";
        } else {
          XLOG(ERR) << "Failed to create ping future";
          kv_client_destroy(client_handle_);
          client_handle_ = nullptr;
          healthy_ = false;
        }
      } else {
        XLOG(ERR) << "No cluster endpoints configured";
        healthy_ = false;
      }
    } catch (const std::exception& e) {
      XLOG(ERR) << "Failed to initialize CustomKvEngine: " << e.what();
      healthy_ = false;
    }
  }

  void cleanup() {
    if (client_handle_) {
      kv_client_destroy(client_handle_);
      client_handle_ = nullptr;
    }
    healthy_ = false;
    XLOG(INFO) << "CustomKvEngine cleaned up";
  }

  std::string generateTransactionId() {
    // Generate a unique transaction ID
    // This could be a UUID or timestamp-based ID
    static std::atomic<uint64_t> counter{0};
    auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return fmt::format("txn_{}_{}_{}",
                      timestamp,
                      thread_id,
                      counter.fetch_add(1));
  }

  CustomKvEngineConfig config_;
  bool healthy_;
  std::shared_ptr<CustomKvEngineImpl> shared_from_this_;
  void* client_handle_{nullptr};  // Rust client handle
  
  // TODO: Add actual client connections here
  // std::unique_ptr<RustKvClient> client_;
  // std::unique_ptr<ConnectionPool> connection_pool_;
};

// CustomKvEngine implementation
CustomKvEngine::CustomKvEngine(const CustomKvEngineConfig& config)
    : impl_(std::make_shared<CustomKvEngineImpl>(config)) {
  impl_->setSharedThis(impl_);
}

CustomKvEngine::~CustomKvEngine() = default;

std::unique_ptr<IReadOnlyTransaction> CustomKvEngine::createReadonlyTransaction() {
  return impl_->createReadonlyTransaction();
}

std::unique_ptr<IReadWriteTransaction> CustomKvEngine::createReadWriteTransaction() {
  return impl_->createReadWriteTransaction();
}

bool CustomKvEngine::isHealthy() const {
  return impl_->isHealthy();
}

void CustomKvEngine::reconnect() {
  impl_->reconnect();
}

void* CustomKvEngine::getClientHandle() const {
  return impl_->getClientHandle();
}

}  // namespace hf3fs::kv