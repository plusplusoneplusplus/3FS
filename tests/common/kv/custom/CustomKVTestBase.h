#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

#include "fdb/CustomKvEngine.h"

namespace hf3fs::kv {

class CustomKVTestBase : public ::testing::Test {
 protected:
  void SetUp() override {
    // Configure the custom KV engine with localhost endpoint
    struct IntegrationTestConfig : public hf3fs::kv::CustomKvEngineConfig {
      IntegrationTestConfig() {
        const_cast<std::vector<std::string>&>(cluster_endpoints()) = {"localhost:9090"};
      }
    };
    
    IntegrationTestConfig config;
    engine_ = std::make_unique<CustomKvEngine>(config);
    
    // Wait a moment for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    engine_.reset();
  }

  void skipIfUnhealthy() {
    if (!engine_->isHealthy()) {
      GTEST_SKIP() << "KV server not available at localhost:9090 - skipping integration test";
    }
  }

 protected:
  std::unique_ptr<CustomKvEngine> engine_;
  
  // Test constants similar to FDB tests
  constexpr static auto testKey = "unittest.foo";
  constexpr static auto testKey2 = "unittest.foo1";
  constexpr static auto testKey3 = "unittest.foo2";
  constexpr static auto testValue = "unittest.bar";
  constexpr static auto conflictKey = "unittest.conflict.";
};

}  // namespace hf3fs::kv