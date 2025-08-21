#include <gtest/gtest.h>

#include "CustomKVTestBase.h"
#include "fdb/CustomKvEngine.h"

namespace hf3fs::kv {

class TestCustomKvEngine : public CustomKVTestBase {};

TEST_F(TestCustomKvEngine, Construct) {
  skipIfUnhealthy();
  ASSERT_TRUE(engine_->createReadonlyTransaction() != nullptr);
  ASSERT_TRUE(engine_->createReadWriteTransaction() != nullptr);
}

TEST_F(TestCustomKvEngine, HealthCheck) {
  skipIfUnhealthy();
  EXPECT_TRUE(engine_->isHealthy());
}


}  // namespace hf3fs::kv