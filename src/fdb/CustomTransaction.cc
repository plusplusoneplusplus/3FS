#include "CustomTransaction.h"

#include <memory>
#include <folly/logging/xlog.h>
#include <fmt/format.h>
#include <folly/experimental/coro/Coroutine.h>
#include <folly/experimental/coro/Baton.h>

#include "common/utils/Status.h"
#include "common/utils/Result.h"

extern "C" {
#include "third_party/kv/rust/src/client/kvstore_client.h"
}

namespace hf3fs::kv {

// Helper function to wait for a future to be ready using callbacks with polling fallback
static folly::coro::Task<void> waitForFuture(KvFutureHandle future) {
  // Check if already ready first
  int ready = kv_future_poll(future);
  if (ready == 1) {
    co_return;
  }
  if (ready == -1) {
    throw std::runtime_error("Future polling failed");
  }

  // Use baton for async notification if not immediately ready
  auto baton = std::make_unique<folly::coro::Baton>();
  auto* baton_ptr = baton.get();

  // Set callback for async notification
  kv_future_set_callback(future,
    [](KvFutureHandle, void* user_context) {
      auto* b = static_cast<folly::coro::Baton*>(user_context);
      b->post();
    },
    baton_ptr);

  // Wait for the callback to be invoked
  co_await *baton;
}

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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(tx_future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(tx_future);
  
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
                                                       0,  // begin_offset
                                                       begin.inclusive ? 0 : 1,  // begin_or_equal
                                                       0,  // end_offset
                                                       end.inclusive ? 1 : 0,    // end_or_equal
                                                       limit,
                                                       nullptr);

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(tx_future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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
  // For simplicity, just delegate to getRange
  co_return co_await getRange(begin, end, limit);
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
                                                  0,  // begin_offset
                                                  begin.inclusive ? 0 : 1,  // begin_or_equal (!inclusive like FDB)
                                                  0,  // end_offset
                                                  end.inclusive ? 1 : 0,    // end_or_equal (inclusive like FDB)
                                                  limit,
                                                  nullptr);

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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
    bool has_more = pair_array.count != 0 && pair_array.count == static_cast<size_t>(limit);
    
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

      try {
        // Wait for future to be ready using pure callback
        co_await waitForFuture(future);

        // Get the void result to clean up
        KvResult result = kv_future_get_void_result(future);
        if (!result.success) {
          XLOG(WARN) << "Transaction abort returned error: "
                     << (result.error_message ? result.error_message : "Unknown error");
        }
        kv_result_free(&result);
      } catch (...) {
        XLOG(WARN) << "Transaction abort callback failed, continuing anyway";
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // The key parameter represents the key prefix in versionstamped operations
  // The offset parameter is ignored in this implementation as the Rust client
  // handles versionstamp placement automatically at the end of the prefix
  
  // Validate inputs before calling the Rust client
  if (key.empty()) {
    XLOG(ERR) << "SetVersionstampedKey: key prefix cannot be empty";
    co_return makeError(StatusCode::kInvalidArg, "SetVersionstampedKey: key prefix cannot be empty");
  }
  
  // Log the operation details for debugging
  XLOG(DBG) << "Calling kv_transaction_set_versionstamped_key with:" 
           << " key_prefix='" << key << "' (" << key.size() << " bytes)" 
           << " value='" << value << "' (" << value.size() << " bytes)" 
           << " transaction_handle=" << transaction_handle_;
  
  int result = kv_transaction_set_versionstamped_key(
      (KvTransactionHandle)transaction_handle_,
      reinterpret_cast<const uint8_t*>(key.data()),
      static_cast<int>(key.size()),
      reinterpret_cast<const uint8_t*>(value.data()),
      static_cast<int>(value.size()),
      nullptr  // no column family
  );
  
  XLOG(DBG) << "kv_transaction_set_versionstamped_key returned: " << result;
  
  if (result != KV_FUNCTION_SUCCESS) {
    XLOG(ERR) << "Set versionstamped key operation failed: key_prefix='" << key 
             << "', value='" << value << "', result=" << result
             << ", transaction_handle=" << transaction_handle_;
    co_return makeError(StatusCode::kIOError, fmt::format(
        "Set versionstamped key operation failed: result={}, key_prefix='{}', value='{}'", 
        result, key, value));
  }
  
  co_return folly::unit;
}

CoTryTask<void> CustomTransaction::setVersionstampedValue(std::string_view key, std::string_view value, uint32_t offset) {
  if (cancelled_.load() || reset_.load() || committed_.load()) {
    co_return makeError(StatusCode::kInvalidArg, "Transaction is finished");
  }

  XLOG(DBG) << "Set versionstamped value for key: " << key << " offset: " << offset 
           << " in transaction: " << transaction_id_;
  
  // Ensure we have a transaction handle
  auto transaction_result = co_await ensureTransaction();
  if (!transaction_result) {
    co_return folly::makeUnexpected(transaction_result.error());
  }
  
  // The value parameter represents the value prefix in versionstamped operations
  // The offset parameter is ignored in this implementation as the Rust client
  // handles versionstamp placement automatically at the end of the prefix

  // Validate inputs before calling the Rust client
  if (key.empty()) {
    XLOG(ERR) << "SetVersionstampedValue: key cannot be empty";
    co_return makeError(StatusCode::kInvalidArg, "SetVersionstampedValue: key cannot be empty");
  }

  // Create a buffer with the prefix + space for 10-byte versionstamp
  // The Rust implementation expects a buffer where the last 10 bytes will be overwritten
  // with the versionstamp, so we need prefix + 10 additional bytes
  std::string value_buffer = std::string(value);
  value_buffer.resize(value.size() + 10, '\0');

  // Log the operation details for debugging
  XLOG(DBG) << "Calling kv_transaction_set_versionstamped_value with:"
           << " key='" << key << "' (" << key.size() << " bytes)"
           << " value_prefix='" << value << "' (" << value.size() << " bytes)"
           << " value_buffer='" << value_buffer << "' (" << value_buffer.size() << " bytes)"
           << " transaction_handle=" << transaction_handle_;

  int result = kv_transaction_set_versionstamped_value(
      (KvTransactionHandle)transaction_handle_,
      reinterpret_cast<const uint8_t*>(key.data()),
      static_cast<int>(key.size()),
      reinterpret_cast<const uint8_t*>(value_buffer.data()),
      static_cast<int>(value_buffer.size()),
      nullptr  // no column family
  );
  
  XLOG(DBG) << "kv_transaction_set_versionstamped_value returned: " << result;
  
  if (result != KV_FUNCTION_SUCCESS) {
    XLOG(ERR) << "Set versionstamped value operation failed: key='" << key 
             << "', value_prefix='" << value << "', result=" << result
             << ", transaction_handle=" << transaction_handle_;
    co_return makeError(StatusCode::kIOError, fmt::format(
        "Set versionstamped value operation failed: result={}, key='{}', value_prefix='{}'", 
        result, key, value));
  }
  
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

  // Wait for future to be ready using pure callback
  co_await waitForFuture(future);
  
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