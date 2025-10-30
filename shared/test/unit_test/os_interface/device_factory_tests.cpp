/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/device_caps_reader_test_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct DeviceFactoryTests : ::testing::Test {
    void SetUp() override {
        ProductConfigHelper productConfigHelper{};
        auto &aotInfos = productConfigHelper.getDeviceAotInfo();

        for (const auto &aotInfo : aotInfos) {
            if (aotInfo.hwInfo->platform.eProductFamily == productFamily) {
                productConfig = aotInfo.aotConfig.value;
                if (!aotInfo.deviceAcronyms.empty()) {
                    productAcronym = aotInfo.deviceAcronyms.front().str();
                } else if (!aotInfo.rtlIdAcronyms.empty()) {
                    productAcronym = aotInfo.rtlIdAcronyms.front().str();
                }
                break;
            }
        }
    }

    DebugManagerStateRestore restore;
    uint32_t productConfig;
    std::string productAcronym;
};

TEST_F(DeviceFactoryTests, givenHwIpVersionOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenCorrectValueIsSet) {
    ExecutionEnvironment executionEnvironment{};
    auto config = defaultHwInfo.get()->ipVersion.value;
    debugManager.flags.OverrideHwIpVersion.set(config);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_EQ(config, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value);
    EXPECT_NE(0u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryTests, givenHwIpVersionOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledWithNumberOfCcsSetToZeroThenFalseIsReturned) {
    ExecutionEnvironment executionEnvironment{};
    auto config = defaultHwInfo.get()->ipVersion.value;
    debugManager.flags.OverrideHwIpVersion.set(config);
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_FALSE(success);
    EXPECT_EQ(config, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value);
    EXPECT_NE(0u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryTests, givenHwIpVersionOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledWithNumberOfCcsSetToZeroColonZeroThenFalseIsReturned) {
    ExecutionEnvironment executionEnvironment{};
    auto config = defaultHwInfo.get()->ipVersion.value;
    debugManager.flags.OverrideHwIpVersion.set(config);
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:0");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_FALSE(success);
    EXPECT_EQ(config, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value);
    EXPECT_NE(0u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryTests, givenHwIpVersionOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenReleaseHelperContainsCorrectIpVersion) {
    ExecutionEnvironment executionEnvironment{};
    auto config = defaultHwInfo.get()->ipVersion.value;
    debugManager.flags.OverrideHwIpVersion.set(config);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    ASSERT_TRUE(success);
    auto *releaseHelper = executionEnvironment.rootDeviceEnvironments[0]->getReleaseHelper();

    if (releaseHelper == nullptr) {
        GTEST_SKIP();
    }
    class ReleaseHelperExpose : public ReleaseHelper {
      public:
        using ReleaseHelper::hardwareIpVersion;
    };

    ReleaseHelperExpose *exposedReleaseHelper = static_cast<ReleaseHelperExpose *>(releaseHelper);
    EXPECT_EQ(config, exposedReleaseHelper->hardwareIpVersion.value);
}

TEST_F(DeviceFactoryTests, givenHwIpVersionAndDeviceIdOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenCorrectValueIsSet) {
    ExecutionEnvironment executionEnvironment{};
    auto config = defaultHwInfo.get()->ipVersion.value;
    debugManager.flags.OverrideHwIpVersion.set(config);
    debugManager.flags.ForceDeviceId.set("0x1234");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_EQ(config, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value);
    EXPECT_EQ(0x1234, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryTests, givenProductFamilyOverrideWhenPrepareDeviceEnvironmentsIsCalledThenCorrectValueIsSet) {
    if (productAcronym.empty()) {
        GTEST_SKIP();
    }
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    debugManager.flags.ProductFamilyOverride.set(productAcronym);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_EQ(productConfig, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value);
}

TEST_F(DeviceFactoryTests, givenHwIpVersionAndProductFamilyOverrideWhenPrepareDeviceEnvironmentsIsCalledThenCorrectValueIsSet) {
    if (productAcronym.empty()) {
        GTEST_SKIP();
    }
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    debugManager.flags.OverrideHwIpVersion.set(0x1234u);
    debugManager.flags.ProductFamilyOverride.set(productAcronym);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_EQ(0x1234u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value);
}

TEST_F(DeviceFactoryTests, givenDisabledRcsWhenPrepareDeviceEnvironmentsCalledThenSetFtrFlag) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    ASSERT_TRUE(success);

    auto releaseHelper = executionEnvironment.rootDeviceEnvironments[0]->getReleaseHelper();

    if (releaseHelper == nullptr) {
        EXPECT_TRUE(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrRcsNode);
    } else {
        EXPECT_NE(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrRcsNode, releaseHelper->isRcsExposureDisabled());
    }
}

TEST_F(DeviceFactoryTests, givenMultipleDevicesWhenInitializeResourcesSucceedsForAtLeastOneDeviceThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(3);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 3u);

    EXPECT_EQ(3u, executionEnvironment.rootDeviceEnvironments.size());
    auto rootDeviceEnvironment0 = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    auto rootDeviceEnvironment1 = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[1].get());
    auto rootDeviceEnvironment2 = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[2].get());

    rootDeviceEnvironment0->initOsInterfaceResults.push_back(true);
    rootDeviceEnvironment0->initOsInterfaceExpectedCallCount = 1u;

    // making rootDeviceEnvironment1 returning false on first call and true on second call
    rootDeviceEnvironment1->initOsInterfaceResults.push_back(false);
    rootDeviceEnvironment1->initOsInterfaceResults.push_back(true);
    rootDeviceEnvironment1->initOsInterfaceExpectedCallCount = 2u;

    rootDeviceEnvironment2->initOsInterfaceExpectedCallCount = 0u;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    ASSERT_TRUE(success);

    EXPECT_EQ(2u, executionEnvironment.rootDeviceEnvironments.size());

    EXPECT_EQ(1u, rootDeviceEnvironment0->initOsInterfaceCalled);
    EXPECT_EQ(2u, rootDeviceEnvironment1->initOsInterfaceCalled);
}

class MockExecutionEnvironmentConfigureCssMode : public MockExecutionEnvironment {
  public:
    using MockExecutionEnvironment::MockExecutionEnvironment;
    using MockExecutionEnvironment::rootDeviceEnvironments;

    void configureCcsMode() override {
        return;
    }
};

TEST_F(DeviceFactoryTests, givenDeviceWhenInitializeResourcesSucceedsButCcsNumberIsZeroThenFalseIsReturned) {
    debugManager.flags.CreateMultipleRootDevices.set(1);
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0");
    MockExecutionEnvironmentConfigureCssMode executionEnvironment(defaultHwInfo.get(), true, 1u);

    EXPECT_EQ(1u, executionEnvironment.rootDeviceEnvironments.size());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    rootDeviceEnvironment->initOsInterfaceResults.push_back(true);

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);
}

TEST_F(DeviceFactoryTests, givenDeviceWhenInitializeResourcesSucceedsButCcsNumberIsZeroColonZeroThenFalseIsReturned) {
    debugManager.flags.CreateMultipleRootDevices.set(1);
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:0");
    MockExecutionEnvironmentConfigureCssMode executionEnvironment(defaultHwInfo.get(), true, 1u);

    EXPECT_EQ(1u, executionEnvironment.rootDeviceEnvironments.size());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    rootDeviceEnvironment->initOsInterfaceResults.push_back(true);

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);
}

TEST_F(DeviceFactoryTests, givenMultipleDevicesWhenInitializeResourcesFailsForAllDevicesThenFailureIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(3);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 3u);

    EXPECT_EQ(3u, executionEnvironment.rootDeviceEnvironments.size());
    auto rootDeviceEnvironment0 = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    auto rootDeviceEnvironment1 = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[1].get());
    auto rootDeviceEnvironment2 = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[2].get());

    // making root device environment failing three times (once per device)
    rootDeviceEnvironment0->initOsInterfaceResults.push_back(false);
    rootDeviceEnvironment0->initOsInterfaceResults.push_back(false);
    rootDeviceEnvironment0->initOsInterfaceResults.push_back(false);
    rootDeviceEnvironment0->initOsInterfaceExpectedCallCount = 3u;
    rootDeviceEnvironment1->initOsInterfaceExpectedCallCount = 0u;
    rootDeviceEnvironment2->initOsInterfaceExpectedCallCount = 0u;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);

    EXPECT_EQ(0u, executionEnvironment.rootDeviceEnvironments.size());
}

TEST_F(DeviceFactoryTests, givenFailedAilInitializationResultWhenPrepareDeviceEnvironmentsIsCalledThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockRootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    mockRootDeviceEnvironment->ailInitializationResult = false;

    bool res = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_FALSE(res);
}

struct DeviceFactoryOverrideTest : public ::testing::Test {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get()};
};

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsIsCalledThenOverrideGpuAddressSpace) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideGpuAddressSpace.set(12);

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(maxNBitValue(12), executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.gpuAddressSpace);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideGpuAddressSpace) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideGpuAddressSpace.set(12);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(maxNBitValue(12), executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.gpuAddressSpace);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsIsCalledThenOverrideRevision) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideRevision.set(3);

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(3u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usRevId);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideRevision) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideRevision.set(3);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(3u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usRevId);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagWithoutZeroXWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideDeviceIdToHexValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceDeviceId.set("1234");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(0x1234u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagWithZeroXWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideDeviceIdToHexValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceDeviceId.set("0x1234");

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(0x1234u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsIsCalledThenOverrideSlmSize) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideSlmSize.set(123);

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(123u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.maxProgrammableSlmSize);
    EXPECT_EQ(123u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.SLMSizeInKb);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideSlmSize) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideSlmSize.set(123);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ(123u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.maxProgrammableSlmSize);
    EXPECT_EQ(123u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.SLMSizeInKb);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsIsCalledThenOverrideRegionCount) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideRegionCount.set(123);

    EXPECT_TRUE(DeviceFactory::prepareDeviceEnvironments(executionEnvironment));

    EXPECT_EQ(123u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.regionCount);
}

TEST_F(DeviceFactoryOverrideTest, givenDebugFlagSetWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenOverrideRegionCount) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideRegionCount.set(123);

    EXPECT_TRUE(DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment));

    EXPECT_EQ(123u, executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.regionCount);
}

TEST_F(DeviceFactoryOverrideTest, givenInvalidPlatformStringWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenFailureIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.ProductFamilyOverride.set("invalid");

    EXPECT_FALSE(DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment));
}

TEST_F(DeviceFactoryOverrideTest, givenFailedProductHelperSetupHardwareInfoWhenPreparingDeviceEnvironmentsForProductFamilyOverrideThenFalseIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int>(CommandStreamReceiverType::tbx));

    struct MyMockProductHelper : MockProductHelper {
        std::unique_ptr<DeviceCapsReader> getDeviceCapsReader(aub_stream::AubManager &aubManager) const override {
            std::vector<uint32_t> caps;
            return std::make_unique<DeviceCapsReaderMock>(caps);
        }
    };

    auto productHelper = new MyMockProductHelper();
    productHelper->setupHardwareInfoResult = false;

    executionEnvironment.rootDeviceEnvironments[0]->productHelper.reset(productHelper);

    auto rc = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_EQ(false, rc);
    EXPECT_EQ(1u, productHelper->setupHardwareInfoCalled);
}

TEST_F(DeviceFactoryOverrideTest, givenDefaultHwInfoWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenSlmSizeInKbEqualsMaxProgrammableSlmSize) {
    DebugManagerStateRestore restore;
    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(hwInfo->capabilityTable.maxProgrammableSlmSize, hwInfo->gtSystemInfo.SLMSizeInKb);
}

HWTEST_F(DeviceFactoryOverrideTest, GivenAubModeWhenValidateDeviceFlagsThenIsProperMessagePrintedAndValueReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::aub));
    std::string expectedMissingProductFamilyStderrSubstr("Missing override for product family, required to set flag ProductFamilyOverride in non hw mode\n");
    std::string expectedMissingHardwareInfoStderrSubstr("Missing override for hardware info, required to set flag HardwareInfoOverride in non hw mode\n");
    auto defaultProductFamily = hardwarePrefix[defaultHwInfo.get()->platform.eProductFamily];
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getProductHelper();
    StreamCapture capture;

    {
        debugManager.flags.ProductFamilyOverride.set("unk");
        debugManager.flags.HardwareInfoOverride.set("default");
        capture.captureStderr();
        EXPECT_FALSE(DeviceFactory::validateDeviceFlags(productHelper));
        auto capturedStderr = capture.getCapturedStderr();

        EXPECT_TRUE(hasSubstr(capturedStderr, expectedMissingProductFamilyStderrSubstr));
        EXPECT_TRUE(hasSubstr(capturedStderr, expectedMissingHardwareInfoStderrSubstr));
    }
    {
        debugManager.flags.ProductFamilyOverride.set(defaultProductFamily);
        debugManager.flags.HardwareInfoOverride.set("default");
        capture.captureStderr();
        EXPECT_FALSE(DeviceFactory::validateDeviceFlags(productHelper));
        auto capturedStderr = capture.getCapturedStderr();
        EXPECT_FALSE(hasSubstr(capturedStderr, expectedMissingProductFamilyStderrSubstr));
        EXPECT_TRUE(hasSubstr(capturedStderr, expectedMissingHardwareInfoStderrSubstr));
    }

    {
        debugManager.flags.ProductFamilyOverride.set("unk");
        debugManager.flags.HardwareInfoOverride.set("1x1x1");
        capture.captureStderr();
        EXPECT_FALSE(DeviceFactory::validateDeviceFlags(productHelper));
        auto capturedStderr = capture.getCapturedStderr();
        EXPECT_TRUE(hasSubstr(capturedStderr, expectedMissingProductFamilyStderrSubstr));
        EXPECT_FALSE(hasSubstr(capturedStderr, expectedMissingHardwareInfoStderrSubstr));
    }

    {

        debugManager.flags.ProductFamilyOverride.set(defaultProductFamily);
        debugManager.flags.HardwareInfoOverride.set("1x1x1");
        capture.captureStderr();
        EXPECT_TRUE(DeviceFactory::validateDeviceFlags(productHelper));
        auto capturedStderr = capture.getCapturedStderr();
        EXPECT_FALSE(hasSubstr(capturedStderr, expectedMissingProductFamilyStderrSubstr));
        EXPECT_FALSE(hasSubstr(capturedStderr, expectedMissingHardwareInfoStderrSubstr));
    }
    {
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::hardware));
        capture.captureStderr();
        EXPECT_TRUE(DeviceFactory::validateDeviceFlags(productHelper));
        auto capturedStderr = capture.getCapturedStderr();
        EXPECT_FALSE(hasSubstr(capturedStderr, expectedMissingProductFamilyStderrSubstr));
        EXPECT_FALSE(hasSubstr(capturedStderr, expectedMissingHardwareInfoStderrSubstr));
    }
}
