#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <map>

#include "CustomKVTestBase.h"
#include "fdb/CustomKvEngine.h"
#include "tests/GtestHelpers.h"

namespace hf3fs::kv {

class TestCustomKvTransaction : public CustomKVTestBase {};

TEST_F(TestCustomKvTransaction, SetValue) {
  failIfNoKvServer();
  
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
  failIfNoKvServer();
  
  const std::string snapKey = "snapshot_test_key";
  const std::string snapValue = "snapshot_test_value";
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    // First, set up the data with a read-write transaction
    {
      auto writeTransaction = engine_->createReadWriteTransaction();
      if (writeTransaction == nullptr) {
        EXPECT_NE(writeTransaction, nullptr) << "Write transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await writeTransaction->set(snapKey, snapValue);
      EXPECT_TRUE(setResult.hasValue()) << "Failed to set up test data";
      
      auto commitResult = co_await writeTransaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit test data";
    }
    
    // Now test snapshotGet with a readonly transaction
    {
      auto readTransaction = engine_->createReadonlyTransaction();
      if (readTransaction == nullptr) {
        EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
        co_return;
      }
      
      auto result = co_await readTransaction->snapshotGet(snapKey);
      EXPECT_TRUE(result.hasValue()) << "SnapshotGet should succeed";
      
      if (result.hasValue() && result.value().has_value()) {
        EXPECT_EQ(result.value().value(), snapValue) << "SnapshotGet should return the committed value";
      } else {
        EXPECT_TRUE(false) << "SnapshotGet should have found the committed data";
      }
      
      auto cancel = co_await readTransaction->cancel();
      EXPECT_TRUE(cancel.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, Get) {
  failIfNoKvServer();
  
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
  failIfNoKvServer();
  
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
  failIfNoKvServer();
  
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
  failIfNoKvServer();
  
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
  failIfNoKvServer();
  
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

TEST_F(TestCustomKvTransaction, SmallBinaryDataSetGet) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    // Test simple ASCII string first to verify the connection works
    {
      std::string key = "ascii_test";
      std::string value = "simple_ascii_value";
      
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await transaction->set(key, value);
      EXPECT_TRUE(setResult.hasValue()) << "Failed to set ASCII data";
      
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit ASCII data";
      
      // Read back with get (instead of snapshotGet)
      auto readTransaction = engine_->createReadWriteTransaction();
      if (readTransaction == nullptr) {
        EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await readTransaction->get(key);
      EXPECT_TRUE(getResult.hasValue()) << "Failed to get ASCII data";
      
      if (getResult.hasValue() && getResult->has_value()) {
        EXPECT_EQ(**getResult, value) << "ASCII data mismatch";
      }
      
      auto cancelResult = co_await readTransaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
    
    const uint32_t seed = 12345;  // Fixed seed for reproducible tests
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint8_t> byteDist(0, 255);
    
    // Store generated data for verification
    std::map<size_t, std::string> testData;
    
    // Test with small binary data sizes
    for (size_t size : {1, 2}) {
      std::string binaryData;
      binaryData.reserve(size);
      
      // Generate random binary data
      for (size_t i = 0; i < size; ++i) {
        binaryData.push_back(static_cast<char>(byteDist(gen)));
      }
      
      testData[size] = binaryData;
      std::string key = "binary_test_small_" + std::to_string(size);
      
      // Set binary data
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await transaction->set(key, binaryData);
      EXPECT_TRUE(setResult.hasValue()) << "Failed to set binary data of size " << size;
      
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit binary data of size " << size;
    }
    
    // Verify all binary data using transaction->get instead of snapshotGet
    for (size_t size : {1, 2}) {
      std::string key = "binary_test_small_" + std::to_string(size);
      
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await transaction->get(key);
      EXPECT_TRUE(getResult.hasValue()) << "Failed to get binary data of size " << size;
      
      if (getResult.hasValue() && getResult->has_value()) {
        // If sizes don't match, log the actual vs expected for debugging
        if (getResult->value().size() != size) {
          XLOG(ERR) << "Size mismatch for binary test size " << size 
                   << ": expected " << size << ", got " << getResult->value().size();
          // Print first few bytes for debugging
          std::string expected_hex, actual_hex;
          for (size_t i = 0; i < std::min(testData[size].size(), size_t(16)); ++i) {
            expected_hex += fmt::format("{:02x} ", static_cast<uint8_t>(testData[size][i]));
          }
          const auto& actual_string = getResult->value();
          for (size_t i = 0; i < std::min(actual_string.size(), size_t(16)); ++i) {
            actual_hex += fmt::format("{:02x} ", static_cast<uint8_t>(actual_string[i]));
          }
          XLOG(ERR) << "Expected first bytes: " << expected_hex;
          XLOG(ERR) << "Actual first bytes: " << actual_hex;
        }
        
        EXPECT_EQ(getResult->value().size(), size) << "Binary data size mismatch for size " << size;
        EXPECT_EQ(**getResult, testData[size]) << "Binary data content mismatch for size " << size;
      }
      
      auto cancelResult = co_await transaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, LargeBinaryDataSetGet) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    const uint32_t seed = 12345;  // Fixed seed for reproducible tests
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint8_t> byteDist(0, 255);
    
    // Store generated data for verification
    std::map<size_t, std::string> testData;
    
    // Test with larger binary data sizes
    for (size_t size : {0, 16, 256, 1024, 4096}) {
      std::string binaryData;
      binaryData.reserve(size);
      
      // Generate random binary data
      for (size_t i = 0; i < size; ++i) {
        binaryData.push_back(static_cast<char>(byteDist(gen)));
      }
      
      testData[size] = binaryData;
      std::string key = "binary_test_large_" + std::to_string(size);
      
      // Set binary data
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await transaction->set(key, binaryData);
      EXPECT_TRUE(setResult.hasValue()) << "Failed to set binary data of size " << size;
      
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit binary data of size " << size;
    }
    
    // Verify all binary data using transaction->get instead of snapshotGet
    for (size_t size : {0, 16, 256, 1024, 4096}) {
      std::string key = "binary_test_large_" + std::to_string(size);
      
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await transaction->get(key);
      EXPECT_TRUE(getResult.hasValue()) << "Failed to get binary data of size " << size;
      
      if (getResult.hasValue() && getResult->has_value()) {
        // If sizes don't match, log the actual vs expected for debugging
        if (getResult->value().size() != size) {
          XLOG(ERR) << "Size mismatch for binary test size " << size 
                   << ": expected " << size << ", got " << getResult->value().size();
          // Print first few bytes for debugging
          std::string expected_hex, actual_hex;
          for (size_t i = 0; i < std::min(testData[size].size(), size_t(16)); ++i) {
            expected_hex += fmt::format("{:02x} ", static_cast<uint8_t>(testData[size][i]));
          }
          const auto& actual_string = getResult->value();
          for (size_t i = 0; i < std::min(actual_string.size(), size_t(16)); ++i) {
            actual_hex += fmt::format("{:02x} ", static_cast<uint8_t>(actual_string[i]));
          }
          XLOG(ERR) << "Expected first bytes: " << expected_hex;
          XLOG(ERR) << "Actual first bytes: " << actual_hex;
        }
        
        EXPECT_EQ(getResult->value().size(), size) << "Binary data size mismatch for size " << size;
        EXPECT_EQ(**getResult, testData[size]) << "Binary data content mismatch for size " << size;
      }
      
      auto cancelResult = co_await transaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, BinaryDataWithNullBytes) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    // Test data with embedded null bytes
    std::vector<std::string> testCases = {
      std::string("\x00", 1),  // Single null byte
      std::string("\x00\x01\x02\x03", 4),  // Null at start
      std::string("\x01\x00\x02\x03", 4),  // Null in middle
      std::string("\x01\x02\x03\x00", 4),  // Null at end
      std::string("\x00\x00\x00\x00", 4),  // All null bytes
      std::string("hello\x00world\x00test", 16),  // Text with nulls
      std::string("\xFF\x00\xFF\x00\xFF", 5),  // Alternating pattern
    };
    
    for (size_t i = 0; i < testCases.size(); ++i) {
      const auto& binaryData = testCases[i];
      std::string key = "null_test_" + std::to_string(i);
      
      // Set binary data with null bytes
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await transaction->set(key, binaryData);
      EXPECT_TRUE(setResult.hasValue()) << "Failed to set binary data with nulls, case " << i;
      
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit binary data with nulls, case " << i;
    }
    
    // Verify all binary data with null bytes
    for (size_t i = 0; i < testCases.size(); ++i) {
      const auto& expectedData = testCases[i];
      std::string key = "null_test_" + std::to_string(i);
      
      auto transaction = engine_->createReadonlyTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await transaction->snapshotGet(key);
      EXPECT_TRUE(getResult.hasValue()) << "Failed to get binary data with nulls, case " << i;
      
      if (getResult.hasValue() && getResult->has_value()) {
        EXPECT_EQ(getResult->value().size(), expectedData.size()) << "Size mismatch for null test case " << i;
        EXPECT_EQ(**getResult, expectedData) << "Content mismatch for null test case " << i;
      }
      
      auto cancelResult = co_await transaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, LargeBinaryData) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> byteDist(0, 255);
    
    // Test large binary data (64KB)
    const size_t largeSize = 64 * 1024;
    std::string largeBinaryData;
    largeBinaryData.reserve(largeSize);
    
    // Generate large random binary data
    for (size_t i = 0; i < largeSize; ++i) {
      largeBinaryData.push_back(static_cast<char>(byteDist(gen)));
    }
    
    std::string key = "large_binary_test";
    
    // Set large binary data
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    auto setResult = co_await transaction->set(key, largeBinaryData);
    EXPECT_TRUE(setResult.hasValue()) << "Failed to set large binary data";
    
    auto commitResult = co_await transaction->commit();
    EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit large binary data";
    
    // Verify large binary data
    auto readTransaction = engine_->createReadonlyTransaction();
    if (readTransaction == nullptr) {
      EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
      co_return;
    }
    
    auto getResult = co_await readTransaction->snapshotGet(key);
    EXPECT_TRUE(getResult.hasValue()) << "Failed to get large binary data";
    
    if (getResult.hasValue() && getResult->has_value()) {
      EXPECT_EQ(getResult->value().size(), largeSize) << "Large binary data size mismatch";
      EXPECT_EQ(**getResult, largeBinaryData) << "Large binary data content mismatch";
    }
    
    auto cancelResult = co_await readTransaction->cancel();
    EXPECT_TRUE(cancelResult.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, BinaryKeyAndValue) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> byteDist(0, 255);
    
    // Generate binary key and value
    std::string binaryKey;
    std::string binaryValue;
    
    const size_t keySize = 32;
    const size_t valueSize = 128;
    
    for (size_t i = 0; i < keySize; ++i) {
      binaryKey.push_back(static_cast<char>(byteDist(gen)));
    }
    
    for (size_t i = 0; i < valueSize; ++i) {
      binaryValue.push_back(static_cast<char>(byteDist(gen)));
    }
    
    // Set binary key-value pair
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    auto setResult = co_await transaction->set(binaryKey, binaryValue);
    EXPECT_TRUE(setResult.hasValue()) << "Failed to set binary key-value pair";
    
    auto commitResult = co_await transaction->commit();
    EXPECT_TRUE(commitResult.hasValue()) << "Failed to commit binary key-value pair";
    
    // Verify binary key-value pair
    auto readTransaction = engine_->createReadonlyTransaction();
    if (readTransaction == nullptr) {
      EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
      co_return;
    }
    
    auto getResult = co_await readTransaction->snapshotGet(binaryKey);
    EXPECT_TRUE(getResult.hasValue()) << "Failed to get binary value by binary key";
    
    if (getResult.hasValue() && getResult->has_value()) {
      EXPECT_EQ(getResult->value().size(), valueSize) << "Binary value size mismatch";
      EXPECT_EQ(**getResult, binaryValue) << "Binary value content mismatch";
    }
    
    auto cancelResult = co_await readTransaction->cancel();
    EXPECT_TRUE(cancelResult.hasValue());
  }());
}

TEST_F(TestCustomKvTransaction, SetVersionstampedKey) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    auto transaction = engine_->createReadWriteTransaction();
    if (transaction == nullptr) {
      EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
      co_return;
    }
    
    // Test versionstamped key operation
    std::string keyPrefix = "user_score_";
    std::string value = "100";
    
    auto result = co_await transaction->setVersionstampedKey(keyPrefix, 0, value);
    EXPECT_TRUE(result.hasValue()) << "SetVersionstampedKey should succeed";
    
    // Commit the transaction
    auto commitResult = co_await transaction->commit();
    EXPECT_TRUE(commitResult.hasValue()) << "Commit should succeed";
    
    // Note: In a full implementation, we would be able to retrieve the generated key
    // from the commit result, but the current interface doesn't support this yet.
    // The test verifies that the operation doesn't fail.
  }());
}

TEST_F(TestCustomKvTransaction, SetVersionstampedValue) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    std::string key = "user_session";
    std::string valuePrefix = "session_";
    
    // Set the versionstamped value
    {
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto result = co_await transaction->setVersionstampedValue(key, valuePrefix, 0);
      EXPECT_TRUE(result.hasValue()) << "SetVersionstampedValue should succeed";
      
      // Commit the transaction
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Commit should succeed";
    }
    
    // Read back the value to verify the versionstamp was appended
    {
      auto readTransaction = engine_->createReadonlyTransaction();
      if (readTransaction == nullptr) {
        EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await readTransaction->snapshotGet(key);
      EXPECT_TRUE(getResult.hasValue()) << "Should be able to read the versionstamped value";
      
      if (getResult.hasValue() && getResult->has_value()) {
        const std::string& actualValue = **getResult;
        
        // The value should start with our prefix
        EXPECT_TRUE(actualValue.starts_with(valuePrefix)) 
            << "Value should start with prefix '" << valuePrefix << "', got: '" << actualValue << "'";
        
        // The value should be longer than just the prefix (should have versionstamp appended)
        EXPECT_GT(actualValue.size(), valuePrefix.size()) 
            << "Value should be longer than prefix, indicating versionstamp was appended";
        
        // The versionstamp should be 10 bytes, so total length should be prefix + 10
        EXPECT_EQ(actualValue.size(), valuePrefix.size() + 10) 
            << "Value should be prefix + 10-byte versionstamp, got length: " << actualValue.size();
        
        XLOG(INFO) << "Successfully verified versionstamped value: prefix='" << valuePrefix 
                   << "', total_length=" << actualValue.size() 
                   << ", versionstamp_length=" << (actualValue.size() - valuePrefix.size());
      } else {
        EXPECT_TRUE(false) << "Should have found the versionstamped value";
      }
      
      auto cancelResult = co_await readTransaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, VersionstampedOperationsInSingleTransaction) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    std::string keyPrefix1 = "event_log_";
    std::string value1 = "user_login";
    std::string key2 = "last_activity";
    std::string valuePrefix2 = "timestamp_";
    std::string regularKey = "regular_key";
    std::string regularValue = "regular_value";
    
    // Perform multiple versionstamped operations in single transaction
    {
      auto transaction = engine_->createReadWriteTransaction();
      if (transaction == nullptr) {
        EXPECT_NE(transaction, nullptr) << "Transaction should not be null";
        co_return;
      }
      
      auto result1 = co_await transaction->setVersionstampedKey(keyPrefix1, 0, value1);
      EXPECT_TRUE(result1.hasValue()) << "First SetVersionstampedKey should succeed";
      
      auto result2 = co_await transaction->setVersionstampedValue(key2, valuePrefix2, 0);
      EXPECT_TRUE(result2.hasValue()) << "SetVersionstampedValue should succeed";
      
      // Add a regular set operation as well
      auto result3 = co_await transaction->set(regularKey, regularValue);
      EXPECT_TRUE(result3.hasValue()) << "Regular set should succeed";
      
      // Commit all operations together
      auto commitResult = co_await transaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Commit should succeed";
    }
    
    // Verify the regular key can be read back normally
    {
      auto readTransaction = engine_->createReadonlyTransaction();
      if (readTransaction == nullptr) {
        EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await readTransaction->snapshotGet(regularKey);
      EXPECT_TRUE(getResult.hasValue()) << "Should be able to read regular key";
      
      if (getResult.hasValue() && getResult->has_value()) {
        EXPECT_EQ(**getResult, regularValue) << "Regular value should match exactly";
      }
      
      // Verify the versionstamped value has the stamp appended
      auto versionedResult = co_await readTransaction->snapshotGet(key2);
      EXPECT_TRUE(versionedResult.hasValue()) << "Should be able to read versionstamped value";
      
      if (versionedResult.hasValue() && versionedResult->has_value()) {
        const std::string& actualValue = **versionedResult;
        EXPECT_TRUE(actualValue.starts_with(valuePrefix2)) 
            << "Versionstamped value should start with prefix";
        EXPECT_EQ(actualValue.size(), valuePrefix2.size() + 10) 
            << "Versionstamped value should be prefix + 10-byte versionstamp";
      }
      
      auto cancelResult = co_await readTransaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

TEST_F(TestCustomKvTransaction, VersionstampedValueReadback) {
  failIfNoKvServer();
  
  folly::coro::blockingWait([&]() -> CoTask<void> {
    // Test that we can read back a versionstamped value and see the stamp
    std::string testKey = "versionstamp_test";
    std::string valuePrefix = "data_";
    
    // Write the versionstamped value
    {
      auto writeTransaction = engine_->createReadWriteTransaction();
      if (writeTransaction == nullptr) {
        EXPECT_NE(writeTransaction, nullptr) << "Write transaction should not be null";
        co_return;
      }
      
      auto setResult = co_await writeTransaction->setVersionstampedValue(testKey, valuePrefix, 0);
      EXPECT_TRUE(setResult.hasValue()) << "SetVersionstampedValue should succeed";
      
      auto commitResult = co_await writeTransaction->commit();
      EXPECT_TRUE(commitResult.hasValue()) << "Commit should succeed";
    }
    
    // Read back and verify the versionstamped value
    {
      auto readTransaction = engine_->createReadonlyTransaction();
      if (readTransaction == nullptr) {
        EXPECT_NE(readTransaction, nullptr) << "Read transaction should not be null";
        co_return;
      }
      
      auto getResult = co_await readTransaction->snapshotGet(testKey);
      EXPECT_TRUE(getResult.hasValue()) << "Should be able to read back the versionstamped value";
      EXPECT_TRUE(getResult->has_value()) << "Key should exist";
      
      if (getResult.hasValue() && getResult->has_value()) {
        const std::string& fullValue = **getResult;
        
        // Verify the structure of the versionstamped value
        EXPECT_GE(fullValue.size(), valuePrefix.size() + 10) 
            << "Value should be at least prefix + 10 bytes for versionstamp";
        
        // Extract the prefix part
        std::string actualPrefix = fullValue.substr(0, valuePrefix.size());
        EXPECT_EQ(actualPrefix, valuePrefix) << "Prefix should match exactly";
        
        // Extract the versionstamp (last 10 bytes)
        if (fullValue.size() >= valuePrefix.size() + 10) {
          std::string versionstamp = fullValue.substr(valuePrefix.size(), 10);
          EXPECT_EQ(versionstamp.size(), 10) << "Versionstamp should be exactly 10 bytes";
          
          // Log the versionstamp bytes for inspection
          std::string hex_stamp;
          for (unsigned char byte : versionstamp) {
            hex_stamp += fmt::format("{:02x} ", byte);
          }
          XLOG(INFO) << "Versionstamp hex: " << hex_stamp;
          
          // Versionstamp should not be all zeros (that would indicate it wasn't set)
          bool all_zeros = std::all_of(versionstamp.begin(), versionstamp.end(), 
                                      [](char c) { return c == '\0'; });
          EXPECT_FALSE(all_zeros) << "Versionstamp should not be all zeros";
          
          XLOG(INFO) << "Successfully verified versionstamped value: " 
                     << "prefix='" << valuePrefix << "', " 
                     << "full_length=" << fullValue.size() << ", "
                     << "versionstamp_length=" << versionstamp.size();
        }
      }
      
      auto cancelResult = co_await readTransaction->cancel();
      EXPECT_TRUE(cancelResult.hasValue());
    }
  }());
}

}  // namespace hf3fs::kv