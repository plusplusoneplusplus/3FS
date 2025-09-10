#include <chrono>

#include "common/app/AppInfo.h"
#include "common/serde/Serde.h"
#include "common/utils/UtcTimeSerde.h"
#include "fbs/mgmtd/MgmtdLeaseInfo.h"
#include "fbs/mgmtd/PersistentNodeInfo.h"
#include "fbs/mgmtd/MgmtdTypes.h"
#include "tests/GtestHelpers.h"

namespace hf3fs::test {
namespace {

TEST(TestFbsSerde, MgmtdLeaseInfo) {
  // Create a sample PersistentNodeInfo
  flat::PersistentNodeInfo nodeInfo;
  nodeInfo.nodeId = flat::NodeId(42);
  nodeInfo.type = flat::NodeType::META;
  nodeInfo.hostname = "test-host.example.com";
  
  // Add a service group
  flat::ServiceGroupInfo serviceGroup({"meta-service-1", "meta-service-2"}, 
                                      {net::Address::fromString("192.168.1.100:9876")});
  nodeInfo.serviceGroups = {serviceGroup};
  
  // Add some tags
  flat::TagPair tag1{"environment", "production"};
  flat::TagPair tag2{"region", "us-west-2"};
  nodeInfo.tags = {tag1, tag2};

  // Create lease timestamps
  auto leaseStartTime = UtcTime::fromMicroseconds(1000000);  // 1 second
  auto leaseEndTime = UtcTime::fromMicroseconds(6000000);    // 6 seconds (5 minute difference)

  // Create the MgmtdLeaseInfo instance
  flat::MgmtdLeaseInfo originalLease(nodeInfo, leaseStartTime, leaseEndTime);
  
  // Verify the original lease has expected values
  ASSERT_EQ(originalLease.primary.nodeId, flat::NodeId(42));
  ASSERT_EQ(originalLease.primary.type, flat::NodeType::META);
  ASSERT_EQ(originalLease.primary.hostname, "test-host.example.com");
  ASSERT_EQ(originalLease.primary.serviceGroups.size(), 1);
  ASSERT_EQ(originalLease.primary.serviceGroups[0].services.size(), 2);
  ASSERT_TRUE(originalLease.primary.serviceGroups[0].services.count("meta-service-1"));
  ASSERT_TRUE(originalLease.primary.serviceGroups[0].services.count("meta-service-2"));
  ASSERT_EQ(originalLease.primary.tags.size(), 2);
  ASSERT_EQ(originalLease.primary.tags[0].key, "environment");
  ASSERT_EQ(originalLease.primary.tags[0].value, "production");
  ASSERT_EQ(originalLease.primary.tags[1].key, "region");
  ASSERT_EQ(originalLease.primary.tags[1].value, "us-west-2");
  ASSERT_EQ(originalLease.leaseStart, leaseStartTime);
  ASSERT_EQ(originalLease.leaseEnd, leaseEndTime);

  // Serialize the lease to memory buffer
  auto serializedData = serde::serialize(originalLease);
  ASSERT_GT(serializedData.size(), 0);
  ASSERT_EQ(serializedData.size(), serde::serializeLength(originalLease));

  // Create a new lease instance for deserialization
  flat::MgmtdLeaseInfo deserializedLease;
  
  // Verify initial state is different
  ASSERT_NE(deserializedLease.primary.nodeId, originalLease.primary.nodeId);
  ASSERT_NE(deserializedLease.primary.type, originalLease.primary.type);
  ASSERT_NE(deserializedLease.primary.hostname, originalLease.primary.hostname);
  ASSERT_NE(deserializedLease.leaseStart, originalLease.leaseStart);
  ASSERT_NE(deserializedLease.leaseEnd, originalLease.leaseEnd);

  // Deserialize the data back into the new instance
  ASSERT_OK(serde::deserialize(deserializedLease, serializedData));

  // Assert that all values match the original
  ASSERT_EQ(deserializedLease.primary.nodeId, originalLease.primary.nodeId);
  ASSERT_EQ(deserializedLease.primary.type, originalLease.primary.type);
  ASSERT_EQ(deserializedLease.primary.hostname, originalLease.primary.hostname);
  ASSERT_EQ(deserializedLease.primary.serviceGroups.size(), originalLease.primary.serviceGroups.size());
  
  // Verify service group details
  ASSERT_EQ(deserializedLease.primary.serviceGroups[0].services.size(), 2);
  ASSERT_TRUE(deserializedLease.primary.serviceGroups[0].services.count("meta-service-1"));
  ASSERT_TRUE(deserializedLease.primary.serviceGroups[0].services.count("meta-service-2"));
  ASSERT_EQ(deserializedLease.primary.serviceGroups[0].endpoints.size(), 1);
  ASSERT_EQ(deserializedLease.primary.serviceGroups[0].endpoints[0].toString(), "TCP://192.168.1.100:9876");
  
  // Verify tags
  ASSERT_EQ(deserializedLease.primary.tags.size(), 2);
  ASSERT_EQ(deserializedLease.primary.tags[0].key, "environment");
  ASSERT_EQ(deserializedLease.primary.tags[0].value, "production");
  ASSERT_EQ(deserializedLease.primary.tags[1].key, "region");
  ASSERT_EQ(deserializedLease.primary.tags[1].value, "us-west-2");
  
  // Verify timestamps
  ASSERT_EQ(deserializedLease.leaseStart, originalLease.leaseStart);
  ASSERT_EQ(deserializedLease.leaseEnd, originalLease.leaseEnd);
  
  // Verify release version
  ASSERT_EQ(deserializedLease.releaseVersion.buildTimeInSeconds, originalLease.releaseVersion.buildTimeInSeconds);
  ASSERT_EQ(deserializedLease.releaseVersion.toString(), originalLease.releaseVersion.toString());

  // Test round-trip serialization to ensure consistency
  auto reSerializedData = serde::serialize(deserializedLease);
  ASSERT_EQ(serializedData.size(), reSerializedData.size());
  ASSERT_EQ(serializedData, reSerializedData);
}

TEST(TestFbsSerde, MgmtdLeaseInfoWithCustomReleaseVersion) {
  // Create a minimal node info
  flat::PersistentNodeInfo nodeInfo;
  nodeInfo.nodeId = flat::NodeId(123);
  nodeInfo.type = flat::NodeType::STORAGE;
  nodeInfo.hostname = "storage-node-1";

  // Create lease timestamps
  auto leaseStart = UtcTime::fromMicroseconds(1000000);
  auto leaseEnd = UtcTime::fromMicroseconds(2000000);
  
  // Create a custom release version
  flat::ReleaseVersion customVersion;
  customVersion.buildTimeInSeconds = 1609459200; // 2021-01-01 00:00:00 UTC

  // Create the MgmtdLeaseInfo instance with custom release version
  flat::MgmtdLeaseInfo originalLease(nodeInfo, leaseStart, leaseEnd, customVersion);
  
  // Verify original values
  ASSERT_EQ(originalLease.primary.nodeId, flat::NodeId(123));
  ASSERT_EQ(originalLease.primary.type, flat::NodeType::STORAGE);
  ASSERT_EQ(originalLease.primary.hostname, "storage-node-1");
  ASSERT_EQ(originalLease.leaseStart, leaseStart);
  ASSERT_EQ(originalLease.leaseEnd, leaseEnd);
  ASSERT_EQ(originalLease.releaseVersion.buildTimeInSeconds, 1609459200);

  // Serialize and deserialize
  auto serializedData = serde::serialize(originalLease);
  flat::MgmtdLeaseInfo deserializedLease;
  ASSERT_OK(serde::deserialize(deserializedLease, serializedData));

  // Assert all values match
  ASSERT_EQ(deserializedLease.primary.nodeId, originalLease.primary.nodeId);
  ASSERT_EQ(deserializedLease.primary.type, originalLease.primary.type);
  ASSERT_EQ(deserializedLease.primary.hostname, originalLease.primary.hostname);
  ASSERT_EQ(deserializedLease.leaseStart, originalLease.leaseStart);
  ASSERT_EQ(deserializedLease.leaseEnd, originalLease.leaseEnd);
  ASSERT_EQ(deserializedLease.releaseVersion.buildTimeInSeconds, originalLease.releaseVersion.buildTimeInSeconds);
  ASSERT_EQ(deserializedLease.releaseVersion.toString(), originalLease.releaseVersion.toString());
}

TEST(TestFbsSerde, MgmtdLeaseInfoEmpty) {
  // Test with minimal/empty data
  flat::PersistentNodeInfo emptyNodeInfo;
  auto zeroTime = UtcTime::fromMicroseconds(0);
  
  flat::MgmtdLeaseInfo originalLease(emptyNodeInfo, zeroTime, zeroTime);
  
  // Verify defaults
  ASSERT_EQ(originalLease.primary.nodeId, flat::NodeId(0));
  ASSERT_EQ(originalLease.primary.type, flat::NodeType::MIN);
  ASSERT_TRUE(originalLease.primary.hostname.empty());
  ASSERT_TRUE(originalLease.primary.serviceGroups.empty());
  ASSERT_TRUE(originalLease.primary.tags.empty());
  ASSERT_EQ(originalLease.leaseStart, zeroTime);
  ASSERT_EQ(originalLease.leaseEnd, zeroTime);

  // Serialize and deserialize
  auto serializedData = serde::serialize(originalLease);
  flat::MgmtdLeaseInfo deserializedLease;
  ASSERT_OK(serde::deserialize(deserializedLease, serializedData));

  // Verify deserialized values match
  ASSERT_EQ(deserializedLease.primary.nodeId, originalLease.primary.nodeId);
  ASSERT_EQ(deserializedLease.primary.type, originalLease.primary.type);
  ASSERT_EQ(deserializedLease.primary.hostname, originalLease.primary.hostname);
  ASSERT_EQ(deserializedLease.primary.serviceGroups.size(), 0);
  ASSERT_EQ(deserializedLease.primary.tags.size(), 0);
  ASSERT_EQ(deserializedLease.leaseStart, originalLease.leaseStart);
  ASSERT_EQ(deserializedLease.leaseEnd, originalLease.leaseEnd);
}

}  // namespace
}  // namespace hf3fs::test