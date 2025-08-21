#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "common/kv/ITransaction.h"
#include "common/utils/Coroutine.h"
#include "common/utils/Result.h"
#include "common/utils/String.h"

namespace hf3fs::kv {

// Forward declarations
class CustomKvEngine;
class CustomKvEngineImpl;

// Read-only transaction implementation for Custom KV
class CustomReadOnlyTransaction : public IReadOnlyTransaction {
 public:
  CustomReadOnlyTransaction(const std::string& transaction_id, 
                           std::shared_ptr<CustomKvEngineImpl> engine,
                           void* client_handle);
  ~CustomReadOnlyTransaction() override;

  // IReadOnlyTransaction interface
  void setReadVersion(int64_t version) override;
  
  CoTryTask<std::optional<String>> snapshotGet(std::string_view key) override;
  CoTryTask<std::optional<String>> get(std::string_view key) override;
  
  CoTryTask<GetRangeResult> snapshotGetRange(const KeySelector& begin,
                                            const KeySelector& end,
                                            int32_t limit) override;
  CoTryTask<GetRangeResult> getRange(const KeySelector& begin,
                                    const KeySelector& end,
                                    int32_t limit) override;
  
  CoTryTask<void> cancel() override;
  void reset() override;

 protected:
  std::string transaction_id_;
  std::shared_ptr<CustomKvEngineImpl> engine_;
  std::optional<int64_t> read_version_;
  std::atomic<bool> cancelled_{false};
  std::atomic<bool> reset_{false};
  
  // Rust client handles
  void* client_handle_{nullptr};
};

// Read-write transaction implementation for Custom KV
class CustomTransaction : public IReadWriteTransaction {
 public:
  CustomTransaction(const std::string& transaction_id,
                   std::shared_ptr<CustomKvEngineImpl> engine,
                   void* client_handle);
  ~CustomTransaction() override;

  // IReadOnlyTransaction interface
  void setReadVersion(int64_t version) override;
  
  CoTryTask<std::optional<String>> snapshotGet(std::string_view key) override;
  CoTryTask<std::optional<String>> get(std::string_view key) override;
  
  CoTryTask<GetRangeResult> snapshotGetRange(const KeySelector& begin,
                                            const KeySelector& end,
                                            int32_t limit) override;
  CoTryTask<GetRangeResult> getRange(const KeySelector& begin,
                                    const KeySelector& end,
                                    int32_t limit) override;
  
  CoTryTask<void> cancel() override;
  void reset() override;

  // IReadWriteTransaction interface
  CoTryTask<void> addReadConflict(std::string_view key) override;
  CoTryTask<void> addReadConflictRange(std::string_view begin, std::string_view end) override;
  
  CoTryTask<void> set(std::string_view key, std::string_view value) override;
  CoTryTask<void> clear(std::string_view key) override;
  
  CoTryTask<void> setVersionstampedKey(std::string_view key, uint32_t offset, std::string_view value) override;
  CoTryTask<void> setVersionstampedValue(std::string_view key, std::string_view value, uint32_t offset) override;
  
  CoTryTask<void> commit() override;
  int64_t getCommittedVersion() override;

 private:
  // Ensure transaction handle is initialized
  CoTryTask<folly::Unit> ensureTransaction();
  
  std::string transaction_id_;
  std::shared_ptr<CustomKvEngineImpl> engine_;
  std::optional<int64_t> read_version_;
  std::atomic<bool> cancelled_{false};
  std::atomic<bool> reset_{false};
  std::atomic<bool> committed_{false};
  std::atomic<int64_t> committed_version_{-1};
  
  // Rust client handles
  void* client_handle_{nullptr};
  void* transaction_handle_{nullptr};
};

}  // namespace hf3fs::kv