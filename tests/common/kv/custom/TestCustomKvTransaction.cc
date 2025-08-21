#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "CustomKVTestBase.h"
#include "fdb/CustomKvEngine.h"
#include "tests/GtestHelpers.h"

namespace hf3fs::kv {

class TestCustomKvTransaction : public CustomKVTestBase {};

TEST_F(TestCustomKvTransaction, SetValue) {
  skipIfUnhealthy();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    auto result = co_await transaction->set(testKey, testValue);
    EXPECT_TRUE(result.hasValue());
    result = co_await transaction->set(testKey2, testValue);
    EXPECT_TRUE(result.hasValue());
    result = co_await transaction->set(testKey3, testValue);
    EXPECT_TRUE(result.hasValue());
    auto commit = co_await transaction->commit();
    EXPECT_TRUE(commit.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, SnapshotGet) {
  skipIfUnhealthy();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    auto transaction = engine_->createReadonlyTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    auto result = co_await transaction->snapshotGet(testKey);
    EXPECT_TRUE(result.hasValue());
    if (result.hasValue() && result.value().has_value()) {
      EXPECT_EQ(result.value().value(), testValue);
    }
    
    auto cancel = co_await transaction->cancel();
    EXPECT_TRUE(cancel.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, Get) {
  skipIfUnhealthy();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    auto result = co_await transaction->get(testKey);
    EXPECT_TRUE(result.hasValue());
    if (result.hasValue() && result.value().has_value()) {
      EXPECT_EQ(result.value().value(), testValue);
    }
    
    auto cancel = co_await transaction->cancel();
    EXPECT_TRUE(cancel.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, Clear) {
  skipIfUnhealthy();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    auto result = co_await transaction->clear(testKey);
    EXPECT_TRUE(result.hasValue());
    result = co_await transaction->clear(testKey2);
    EXPECT_TRUE(result.hasValue());
    result = co_await transaction->clear(testKey3);
    EXPECT_TRUE(result.hasValue());
    auto commit = co_await transaction->commit();
    EXPECT_TRUE(commit.hasValue());
    auto cancel = co_await transaction->cancel();
    EXPECT_TRUE(cancel.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, MultipleKeysTransaction) {
  skipIfUnhealthy();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    // Write multiple key-value pairs
    auto setResult1 = co_await transaction->set(testKey, testValue);
    EXPECT_TRUE(setResult1.hasValue());
    
    auto setResult2 = co_await transaction->set(testKey2, testValue);
    EXPECT_TRUE(setResult2.hasValue());
    
    // Read both back
    auto getResult1 = co_await transaction->get(testKey);
    EXPECT_TRUE(getResult1.hasValue());
    if (getResult1.hasValue() && getResult1->has_value()) {
      EXPECT_EQ(**getResult1, testValue);
    }
    
    auto getResult2 = co_await transaction->get(testKey2);
    EXPECT_TRUE(getResult2.hasValue());
    if (getResult2.hasValue() && getResult2->has_value()) {
      EXPECT_EQ(**getResult2, testValue);
    }
    
    // Commit all changes
    auto commitResult = co_await transaction->commit();
    EXPECT_TRUE(commitResult.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, ReadAfterCommit) {
  skipIfUnhealthy();
  
  const std::string key = "read_after_commit_key";
  const std::string value = "read_after_commit_value";
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    // First transaction: write and commit
    {
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await transaction->set(key, value);
      EXPECT_TRUE(setResult.hasValue());
      
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue());
    }
    
    // Second transaction: read the committed value
    {
      auto transaction = engine_->createReadonlyTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await transaction->snapshotGet(key);
      EXPECT_TRUE(getResult.hasValue()) << "Snapshot get should succeed";
      
      if (getResult.hasValue() && getResult->has_value()) {
        EXPECT_EQ(**getResult, value) << "Should read committed value from previous transaction";
      }
      
      auto cancelResult = co_await transaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, TransactionAbort) {
  skipIfUnhealthy();
  
  const std::string key = "abort_test_key";
  const std::string value = "abort_test_value";
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    // First transaction: write but don't commit
    {
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await transaction->set(key, value);
      EXPECT_TRUE(setResult.hasValue());
      
      // Cancel instead of commit
      auto cancelResult = co_await transaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
    
    // Second transaction: verify the key was not persisted
    {
      auto transaction = engine_->createReadonlyTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await transaction->snapshotGet(key);
      EXPECT_TRUE(getResult.hasValue());
      
      if (getResult.hasValue()) {
        EXPECT_FALSE(getResult->has_value()) << "Key should not exist after transaction was canceled";
      }
      
      auto cancelResult = co_await transaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

}  // namespace hf3fs::kv