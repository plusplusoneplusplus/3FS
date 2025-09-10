#include "CustomTransaction.h"

#include <folly/logging/xlog.h>
#include <fmt/format.h>

#include "common/utils/Status.h"
#include "common/utils/Result.h"

extern "C" {
#include "third_party/kv/rust/client/include/kvstore_client.h"
}

namespace hf3fs::kv {

// CustomReadOnlyTransaction implementation
CustomReadOnlyTransaction::CustomReadOnlyTransaction(const std::string& transaction_id,
                                                   std::shared_ptr<CustomKvEngineImpl> engine,
                                                   void* client_handle)
    : transaction_id_(transaction_id), engine_(std::move(engine)), client_handle_(client_handle) {
  XLOG(DBG) << "Created readonly transaction: " << transaction_id_;
}

CustomReadOnlyTransaction::~CustomReadOnlyTransaction() {
  if (!cancelled_.load() && !reset_.load()) {
    // Auto-cancel on destruction
    try {
      auto cancel_task = cancel();
      // Note: In real implementation, we might need to handle this differently
      // since we can't easily await in destructor
    } catch (const std::exception& e) {
      XLOG(ERR) << "Failed to cancel transaction in destructor: " << e.what();
    }
  }
  XLOG(DBG) << "Destroyed readonly transaction: " << transaction_id_;
}

void CustomReadOnlyTransaction::setReadVersion(int64_t version) {
  read_version_ = version;
  XLOG(DBG) << "Set read version " << version << " for transaction: " << transaction_id_;
}

CoTryTask<std::optional<String>> CustomReadOnlyTransaction::snapshotGet(std::string_view key) {
  if (cancelled_.load() || reset_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is cancelled or reset");
  }

  XLOG(DBG) << "Snapshot get key: " << key << " in transaction: " << transaction_id_;
  
  // For snapshot reads on readonly transactions, we use a dedicated read transaction
  // This provides snapshot isolation without the overhead of a full read-write transaction
  if (!client_handle_) {
    co_return makeError(StatusCode::kIOError, "Client handle not available");
  }
  
  // Begin a read transaction with specific version if set
  KvFutureHandle tx_future = kv_read_transaction_begin(client_handle_, read_version_.value_or(0));
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(tx_future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for read transaction begin";
      co_return makeError(StatusCode::kIOError, "Read transaction begin failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Read transaction begin timed out";
    co_return makeError(RPCCode::kTimeout, "Read transaction begin timed out");
  }
  
  // Get the read transaction handle
  KvReadTransactionHandle read_tx = kv_future_get_read_transaction(tx_future);
  if (!read_tx) {
    XLOG(ERR) << "Failed to get read transaction handle";
    co_return makeError(StatusCode::kIOError, "Failed to get read transaction handle");
  }
  
  // Call async get operation on read transaction
  KvFutureHandle future = kv_read_transaction_get(read_tx, 
                                                 reinterpret_cast<const uint8_t*>(key.data()),
                                                 static_cast<int>(key.size()),
                                                 nullptr);
  
  // Poll until ready
  ready = 0;
  retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for snapshot get operation";
      kv_read_transaction_destroy(read_tx);
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Snapshot get operation timed out";
    kv_read_transaction_destroy(read_tx);
    co_return makeError(RPCCode::kTimeout, "Snapshot get operation timed out");
  }
  
  // Get the result
  KvBinaryData value;
  KvResult result = kv_future_get_value_result(future, &value);
  
  if (result.success && value.data) {
    std::string result_str;
    result_str.assign(reinterpret_cast<const char*>(value.data), value.length);
    kv_binary_free(&value);
    
    // Clean up read transaction
    kv_read_transaction_destroy(read_tx);
    
    co_return result_str;
  } else {
    if (result.error_message) {
      XLOG(DBG) << "Key not found or error in snapshot read: " << result.error_message;
      kv_result_free(&result);
    }
    
    // Clean up read transaction
    kv_read_transaction_destroy(read_tx);
    
    co_return std::nullopt;
  }
}

CoTryTask<std::optional<String>> CustomReadOnlyTransaction::get(std::string_view key) {
  if (cancelled_.load() || reset_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is cancelled or reset");
  }

  XLOG(DBG) << "Get key: " << key << " in transaction: " << transaction_id_;
  
  // TODO: Implement actual call to Rust client
  // This would involve:
  // 1. Call the Rust KV server's get method with conflict detection
  // 2. Pass the transaction_id and key
  // 3. Handle the response and convert to 3FS types
  // 4. Handle errors and conflicts appropriately
  
  // Placeholder implementation
  co_return std::nullopt;
}

CoTryTask<IReadOnlyTransaction::GetRangeResult> CustomReadOnlyTransaction::snapshotGetRange(
    const KeySelector& begin, const KeySelector& end, int32_t limit) {
  if (cancelled_.load() || reset_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is cancelled or reset");
  }

  XLOG(DBG) << "Snapshot get range from: " << begin.key << " to: " << end.key 
           << " limit: " << limit << " in transaction: " << transaction_id_;
  
  // For read-only transactions, we'll use the regular get_range since the KV store
  // handles isolation internally. In a production implementation, we might create
  // a read-only transaction handle, but for now we'll use the regular client interface.
  
  if (!client_handle_) {
    co_return makeError(StatusCode::kIOError, "Client handle not available");
  }
  
  // Begin a read transaction with specific version if set
  KvFutureHandle tx_future = kv_read_transaction_begin(client_handle_, read_version_.value_or(0));
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(tx_future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for read transaction begin";
      co_return makeError(StatusCode::kIOError, "Read transaction begin failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Read transaction begin timed out";
    co_return makeError(RPCCode::kTimeout, "Read transaction begin timed out");
  }
  
  // Get the read transaction handle
  KvReadTransactionHandle read_tx = kv_future_get_read_transaction(tx_future);
  if (!read_tx) {
    XLOG(ERR) << "Failed to get read transaction handle";
    co_return makeError(StatusCode::kIOError, "Failed to get read transaction handle");
  }
  
  // Call async get_range operation on read transaction
  std::string start_key(begin.key);
  std::string end_key(end.key);
  KvFutureHandle future = kv_read_transaction_get_range(read_tx, 
                                                       reinterpret_cast<const uint8_t*>(start_key.data()),
                                                       static_cast<int>(start_key.length()),
                                                       reinterpret_cast<const uint8_t*>(end_key.data()), 
                                                       static_cast<int>(end_key.length()),
                                                       limit, 
                                                       nullptr);
  
  // Poll until ready
  ready = 0;
  retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for snapshot get_range operation";
      kv_read_transaction_destroy(read_tx);
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Snapshot get range operation timed out";
    kv_read_transaction_destroy(read_tx);
    co_return makeError(RPCCode::kTimeout, "Snapshot get range operation timed out");
  }
  
  // Get the result
  KvPairArray pair_array;
  KvResult result = kv_future_get_kv_array_result(future, &pair_array);
  
  std::vector<KeyValue> kvs;
  
  if (result.success) {
    for (size_t i = 0; i < pair_array.count; i++) {
      std::string key_str(reinterpret_cast<const char*>(pair_array.pairs[i].key.data), 
                         pair_array.pairs[i].key.length);
      std::string value_str;
      value_str.assign(reinterpret_cast<const char*>(pair_array.pairs[i].value.data), 
                       pair_array.pairs[i].value.length);
      kvs.emplace_back(std::move(key_str), std::move(value_str));
    }
    kv_pair_array_free(&pair_array);
    
    // Check if we have more results
    bool has_more = pair_array.count == static_cast<size_t>(limit);
    
    // Clean up read transaction
    kv_read_transaction_destroy(read_tx);
    
    co_return GetRangeResult{std::move(kvs), has_more};
  } else {
    std::string error_msg = result.error_message ? result.error_message : "Unknown error";
    XLOG(ERR) << "Snapshot get range operation failed: " << error_msg;
    kv_result_free(&result);
    kv_read_transaction_destroy(read_tx);
    co_return makeError(StatusCode::kIOError, fmt::format("Snapshot get range operation failed: {}", error_msg));
  }
}

CoTryTask<IReadOnlyTransaction::GetRangeResult> CustomReadOnlyTransaction::getRange(
    const KeySelector& begin, const KeySelector& end, int32_t limit) {
  if (cancelled_.load() || reset_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is cancelled or reset");
  }

  XLOG(DBG) << "Get range from: " << begin.key << " to: " << end.key 
           << " limit: " << limit << " in transaction: " << transaction_id_;
  
  // TODO: Implement actual call to Rust client with conflict detection
  std::vector<KeyValue> kvs;
  co_return GetRangeResult{std::move(kvs), false};
}

CoTryTask<void> CustomReadOnlyTransaction::cancel() {
  bool expected = false;
  if (cancelled_.compare_exchange_strong(expected, true)) {
    XLOG(DBG) << "Cancelling transaction: " << transaction_id_;
    
    // TODO: Implement actual call to Rust client to abort transaction
    // This would call the abort_transaction method on the Rust server
  }
  co_return folly::unit;
}

void CustomReadOnlyTransaction::reset() {
  reset_.store(true);
  cancelled_.store(false);
  read_version_.reset();
  XLOG(DBG) << "Reset transaction: " << transaction_id_;
  
  // TODO: Reset any internal state for the Rust client
}

// CustomTransaction implementation
CustomTransaction::CustomTransaction(const std::string& transaction_id,
                                   std::shared_ptr<CustomKvEngineImpl> engine,
                                   void* client_handle)
    : transaction_id_(transaction_id), engine_(std::move(engine)), client_handle_(client_handle) {
  XLOG(DBG) << "Created read-write transaction: " << transaction_id_;
}

CustomTransaction::~CustomTransaction() {
  if (!cancelled_.load() && !reset_.load() && !committed_.load()) {
    // Auto-cancel on destruction if not committed
    try {
      auto cancel_task = cancel();
      // Note: In real implementation, we might need to handle this differently
    } catch (const std::exception& e) {
      XLOG(ERR) << "Failed to cancel transaction in destructor: " << e.what();
    }
  }
  XLOG(DBG) << "Destroyed read-write transaction: " << transaction_id_;
}

CoTryTask<folly::Unit> CustomTransaction::ensureTransaction() {
  if (transaction_handle_) {
    co_return folly::unit;
  }

  // Check client handle is available
  if (!client_handle_) {
    co_return makeError(StatusCode::kIOError, "Client handle not available");
  }

  // Begin a new transaction
  KvFutureHandle future = kv_transaction_begin(client_handle_, 30);  // 30 seconds timeout
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for transaction begin";
      co_return makeError(StatusCode::kIOError, "Transaction begin failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Transaction begin timed out";
    co_return makeError(RPCCode::kTimeout, "Transaction begin timed out");
  }
  
  // Get the transaction handle
  transaction_handle_ = kv_future_get_transaction(future);
  if (!transaction_handle_) {
    XLOG(ERR) << "Failed to get transaction handle";
    co_return makeError(StatusCode::kIOError, "Failed to get transaction handle");
  }
  
  XLOG(DBG) << "Transaction initialized: " << transaction_id_;
  co_return folly::unit;
}

void CustomTransaction::setReadVersion(int64_t version) {
  read_version_ = version;
  XLOG(DBG) << "Set read version " << version << " for transaction: " << transaction_id_;
}

CoTryTask<std::optional<String>> CustomTransaction::snapshotGet(std::string_view key) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Snapshot get key: " << key << " in transaction: " << transaction_id_;
  
  // For snapshot reads, we bypass local uncommitted writes and read directly from 
  // the database at the transaction's read version using a read transaction
  if (!client_handle_) {
    co_return makeError(StatusCode::kIOError, "Client handle not available");
  }
  
  // Begin a read transaction with specific version if set
  KvFutureHandle tx_future = kv_read_transaction_begin(client_handle_, read_version_.value_or(0));
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(tx_future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for read transaction begin";
      co_return makeError(StatusCode::kIOError, "Read transaction begin failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Read transaction begin timed out";
    co_return makeError(RPCCode::kTimeout, "Read transaction begin timed out");
  }
  
  // Get the read transaction handle
  KvReadTransactionHandle read_tx = kv_future_get_read_transaction(tx_future);
  if (!read_tx) {
    XLOG(ERR) << "Failed to get read transaction handle";
    co_return makeError(StatusCode::kIOError, "Failed to get read transaction handle");
  }
  
  // Call async get operation on read transaction
  KvFutureHandle future = kv_read_transaction_get(read_tx, 
                                                 reinterpret_cast<const uint8_t*>(key.data()),
                                                 static_cast<int>(key.size()),
                                                 nullptr);
  
  // Poll until ready
  ready = 0;
  retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for snapshot get operation";
      kv_read_transaction_destroy(read_tx);
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Snapshot get operation timed out";
    kv_read_transaction_destroy(read_tx);
    co_return makeError(RPCCode::kTimeout, "Snapshot get operation timed out");
  }
  
  // Get the result
  KvBinaryData value;
  KvResult result = kv_future_get_value_result(future, &value);
  
  if (result.success && value.data) {
    std::string result_str;
    result_str.assign(reinterpret_cast<const char*>(value.data), value.length);
    kv_binary_free(&value);
    
    // Clean up read transaction
    kv_read_transaction_destroy(read_tx);
    
    co_return result_str;
  } else {
    if (result.error_message) {
      XLOG(DBG) << "Key not found or error in snapshot read: " << result.error_message;
      kv_result_free(&result);
    }
    
    // Clean up read transaction
    kv_read_transaction_destroy(read_tx);
    
    co_return std::nullopt;
  }
}

CoTryTask<std::optional<String>> CustomTransaction::get(std::string_view key) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Get key: " << key << " in transaction: " << transaction_id_;
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // Call async get operation
  KvFutureHandle future = kv_transaction_get((KvTransactionHandle)transaction_handle_, 
                                            reinterpret_cast<const uint8_t*>(key.data()),
                                            static_cast<int>(key.size()),
                                            nullptr);
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for get operation";
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Get operation timed out";
    co_return makeError(RPCCode::kTimeout, "Get operation timed out");
  }
  
  // Get the result
  KvBinaryData value;
  KvResult result = kv_future_get_value_result(future, &value);
  
  if (result.success && value.data) {
    std::string result_str;
    result_str.assign(reinterpret_cast<const char*>(value.data), value.length);
    kv_binary_free(&value);
    co_return result_str;
  } else {
    if (result.error_message) {
      XLOG(DBG) << "Key not found or error: " << result.error_message;
      kv_result_free(&result);
    }
    co_return std::nullopt;
  }
}

CoTryTask<IReadOnlyTransaction::GetRangeResult> CustomTransaction::snapshotGetRange(
    const KeySelector& begin, const KeySelector& end, int32_t limit) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  // TODO: Implement actual call to Rust client
  std::vector<KeyValue> kvs;
  co_return GetRangeResult{std::move(kvs), false};
}

CoTryTask<IReadOnlyTransaction::GetRangeResult> CustomTransaction::getRange(
    const KeySelector& begin, const KeySelector& end, int32_t limit) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Get range from: " << begin.key << " to: " << end.key 
           << " limit: " << limit << " in transaction: " << transaction_id_;
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // Call async get_range operation
  std::string start_key(begin.key);
  std::string end_key(end.key);
  KvFutureHandle future = kv_transaction_get_range((KvTransactionHandle)transaction_handle_, 
                                                  reinterpret_cast<const uint8_t*>(start_key.data()),
                                                  static_cast<int>(start_key.length()),
                                                  reinterpret_cast<const uint8_t*>(end_key.data()),
                                                  static_cast<int>(end_key.length()),
                                                  limit, 
                                                  nullptr);
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for get_range operation";
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Get range operation timed out";
    co_return makeError(RPCCode::kTimeout, "Get range operation timed out");
  }
  
  // Get the result
  KvPairArray pair_array;
  KvResult result = kv_future_get_kv_array_result(future, &pair_array);
  
  std::vector<KeyValue> kvs;
  
  if (result.success) {
    for (size_t i = 0; i < pair_array.count; i++) {
      std::string key_str(reinterpret_cast<const char*>(pair_array.pairs[i].key.data), 
                         pair_array.pairs[i].key.length);
      std::string value_str;
      value_str.assign(reinterpret_cast<const char*>(pair_array.pairs[i].value.data), 
                       pair_array.pairs[i].value.length);
      kvs.emplace_back(std::move(key_str), std::move(value_str));
    }
    kv_pair_array_free(&pair_array);
    
    // Check if we have more results (simplified: if we got exactly the limit, assume more)
    bool has_more = pair_array.count == static_cast<size_t>(limit);
    
    co_return GetRangeResult{std::move(kvs), has_more};
  } else {
    std::string error_msg = result.error_message ? result.error_message : "Unknown error";
    XLOG(ERR) << "Get range operation failed: " << error_msg;
    kv_result_free(&result);
    co_return makeError(StatusCode::kIOError, fmt::format("Get range operation failed: {}", error_msg));
  }
}

CoTryTask<void> CustomTransaction::cancel() {
  bool expected = false;
  if (cancelled_.compare_exchange_strong(expected, true)) {
    XLOG(DBG) << "Cancelling transaction: " << transaction_id_;
    
    // If we have a transaction handle, abort it
    if (transaction_handle_) {
      KvFutureHandle future = kv_transaction_abort((KvTransactionHandle)transaction_handle_);
      
      // Poll until ready (simple blocking approach)
      int ready = 0;
      int retries = 0;
      while (ready != 1 && retries < 50) {  // Shorter timeout for abort
        ready = kv_future_poll(future);
        if (ready == -1) {
          XLOG(WARN) << "Future polling failed for abort operation, continuing anyway";
          break;
        }
        if (ready != 1) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          retries++;
        }
      }
      
      if (ready == 1) {
        // Get the void result to clean up
        KvResult result = kv_future_get_void_result(future);
        if (!result.success) {
          XLOG(WARN) << "Transaction abort returned error: " 
                     << (result.error_message ? result.error_message : "Unknown error");
        }
        kv_result_free(&result);
      }
      
      transaction_handle_ = nullptr;
    }
  }
  co_return folly::unit;
}

void CustomTransaction::reset() {
  reset_.store(true);
  cancelled_.store(false);
  committed_.store(false);
  committed_version_.store(-1);
  read_version_.reset();
  XLOG(DBG) << "Reset transaction: " << transaction_id_;
  
  // TODO: Reset any internal state for the Rust client
}

CoTryTask<void> CustomTransaction::addReadConflict(std::string_view key) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Add read conflict for key: " << key << " in transaction: " << transaction_id_;
  
  // TODO: Implement actual call to Rust client to add read conflict
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::addReadConflictRange(std::string_view begin, std::string_view end) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Add read conflict range from: " << begin << " to: " << end 
           << " in transaction: " << transaction_id_;
  
  // TODO: Implement actual call to Rust client to add read conflict range
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::set(std::string_view key, std::string_view value) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Set key: " << key << " value: " << value << " in transaction: " << transaction_id_;
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // Call async set operation
  KvFutureHandle future = kv_transaction_set((KvTransactionHandle)transaction_handle_, 
                                            reinterpret_cast<const uint8_t*>(key.data()),
                                            static_cast<int>(key.size()),
                                            reinterpret_cast<const uint8_t*>(value.data()),
                                            static_cast<int>(value.size()),
                                            nullptr);
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for set operation";
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Set operation timed out";
    co_return makeError(RPCCode::kTimeout, "Set operation timed out");
  }
  
  // Get the result
  KvResult result = kv_future_get_void_result(future);
  
  if (!result.success) {
    std::string error_msg = result.error_message ? result.error_message : "Unknown error";
    XLOG(ERR) << "Set operation failed: " << error_msg;
    kv_result_free(&result);
    co_return makeError(StatusCode::kIOError, fmt::format("Set operation failed: {}", error_msg));
  }
  
  kv_result_free(&result);
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::clear(std::string_view key) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Clear key: " << key << " in transaction: " << transaction_id_;
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // Call async delete operation
  KvFutureHandle future = kv_transaction_delete((KvTransactionHandle)transaction_handle_, 
                                               reinterpret_cast<const uint8_t*>(key.data()),
                                               static_cast<int>(key.size()),
                                               nullptr);
  
  // Poll until ready
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for delete operation";
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Delete operation timed out";
    co_return makeError(RPCCode::kTimeout, "Delete operation timed out");
  }
  
  // Get the result
  KvResult result = kv_future_get_void_result(future);
  
  if (!result.success) {
    std::string error_msg = result.error_message ? result.error_message : "Unknown error";
    XLOG(ERR) << "Delete operation failed: " << error_msg;
    kv_result_free(&result);
    co_return makeError(StatusCode::kIOError, fmt::format("Delete operation failed: {}", error_msg));
  }
  
  kv_result_free(&result);
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::setVersionstampedKey(std::string_view key, uint32_t offset, std::string_view value) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Set versionstamped key: " << key << " offset: " << offset 
           << " in transaction: " << transaction_id_;
  
  // TODO: Implement actual call to Rust client for versionstamped key
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::setVersionstampedValue(std::string_view key, std::string_view value, uint32_t offset) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Set versionstamped value for key: " << key << " offset: " << offset 
           << " in transaction: " << transaction_id_;
  
  // TODO: Implement actual call to Rust client for versionstamped value
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::commit() {
  if (cancelled_.load() || reset_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is cancelled or reset");
  }

  bool expected = false;
  if (!committed_.compare_exchange_strong(expected, true)) {
    // Already committed
    co_return folly::unit;
  }

  XLOG(DBG) << "Committing transaction: " << transaction_id_;
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // Call async commit operation
  KvFutureHandle future = kv_transaction_commit((KvTransactionHandle)transaction_handle_);
  
  // Poll until ready (simple blocking approach)
  int ready = 0;
  int retries = 0;
  while (ready != 1 && retries < 100) {
    ready = kv_future_poll(future);
    if (ready == -1) {
      XLOG(ERR) << "Future polling failed for commit operation";
      co_return makeError(StatusCode::kIOError, "Future polling failed");
    }
    if (ready != 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      retries++;
    }
  }
  
  if (ready != 1) {
    XLOG(ERR) << "Commit operation timed out after " << retries << " retries";
    co_return makeError(RPCCode::kTimeout, "Commit operation timed out");
  }
  
  // Get the void result
  KvResult result = kv_future_get_void_result(future);
  
  if (!result.success) {
    XLOG(ERR) << "Commit failed: " << (result.error_message ? result.error_message : "Unknown error");
    status_code_t error_code = StatusCode::kIOError;
    
    // Map KV error codes to status codes
    switch (result.error_code) {
      case KV_ERROR_TRANSACTION_CONFLICT:
        error_code = TransactionCode::kConflict;
        break;
      case KV_ERROR_TRANSACTION_TIMEOUT:
        error_code = RPCCode::kTimeout;
        break;
      case KV_ERROR_TRANSACTION_NOT_FOUND:
        error_code = StatusCode::kInvalidArg;
        break;
      default:
        error_code = StatusCode::kIOError;
        break;
    }
    
    std::string error_msg = result.error_message ? result.error_message : "Commit failed";
    kv_result_free(&result);
    co_return makeError(error_code, error_msg);
  }
  
  kv_result_free(&result);
  
  // Set committed version to current timestamp
  committed_version_.store(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
  
  XLOG(DBG) << "Transaction committed successfully: " << transaction_id_;
  co_return folly::unit;
}

int64_t CustomTransaction::getCommittedVersion() {
  return committed_version_.load();
}

}  // namespace hf3fs::kv