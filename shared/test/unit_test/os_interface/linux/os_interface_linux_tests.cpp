/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern GT_SYSTEM_INFO passedGtSystemInfo;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

TEST(OsInterfaceTest, GivenLinuxWhenCallingAre64kbPagesEnabledThenReturnFalse) {
    EXPECT_FALSE(OSInterface::are64kbPagesEnabled());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenDeviceHandleQueriedThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    OSInterface osInterface;
    osInterface.setDriverModel(std::move(drm));
    EXPECT_EQ(0u, osInterface.getDriverModel()->getDeviceHandle());
}

TEST(OsInterfaceTest, GivenLinuxOsWhenCheckForNewResourceImplicitFlushSupportThenReturnTrue) {
    EXPECT_TRUE(OSInterface::newResourceImplicitFlush);
}

TEST(OsInterfaceTest, GivenLinuxOsWhenCheckForGpuIdleImplicitFlushSupportThenReturnFalse) {
    EXPECT_TRUE(OSInterface::gpuIdleImplicitFlush);
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenCallingIsDebugAttachAvailableThenFalseIsReturned) {
    OSInterface osInterface;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    EXPECT_FALSE(osInterface.isDebugAttachAvailable());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenCallingGetAggregatedProcessCountThenCallRedirectedToDriverModel) {
    OSInterface osInterface;
    EXPECT_EQ(0u, osInterface.getAggregatedProcessCount());

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    drm->mockProcessCount = 5;
    EXPECT_EQ(5u, osInterface.getAggregatedProcessCount());
}

TEST(OsInterfaceTest, whenOsInterfaceSetupsGmmInputArgsThenFileDescriptorIsSetWithValueOfAdapterBdf) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    auto drm = std::make_unique<DrmMock>(rootDeviceEnvironment);
    drm->setPciPath("0000:01:23.4");
    EXPECT_EQ(0, drm->queryAdapterBDF());

    GMM_INIT_IN_ARGS gmmInputArgs = {};
#if defined(__linux__)
    uint32_t &fileDescriptor = gmmInputArgs.FileDescriptor;
#else
    uint32_t &fileDescriptor = gmmInputArgs.stAdapterBDF.Data;
#endif

    EXPECT_EQ(0u, fileDescriptor);
    drm->Drm::setGmmInputArgs(&gmmInputArgs);
    EXPECT_NE(0u, fileDescriptor);

    ADAPTER_BDF expectedAdapterBDF{};
    expectedAdapterBDF.Bus = 0x1;
    expectedAdapterBDF.Device = 0x23;
    expectedAdapterBDF.Function = 0x4;
    EXPECT_EQ(expectedAdapterBDF.Data, fileDescriptor);
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenGetThresholdForStagingCalledThenReturnThresholdForIntegratedDevices) {
    OSInterface osInterface;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    auto alignedPtr = reinterpret_cast<const void *>(2 * MemoryConstants::megaByte);
    EXPECT_TRUE(osInterface.isSizeWithinThresholdForStaging(alignedPtr, 16 * MemoryConstants::megaByte));
    EXPECT_FALSE(osInterface.isSizeWithinThresholdForStaging(alignedPtr, 64 * MemoryConstants::megaByte));

    auto misalignedPtr = reinterpret_cast<const void *>(3 * MemoryConstants::megaByte);
    EXPECT_TRUE(osInterface.isSizeWithinThresholdForStaging(misalignedPtr, 64 * MemoryConstants::megaByte));
    EXPECT_FALSE(osInterface.isSizeWithinThresholdForStaging(misalignedPtr, 512 * MemoryConstants::megaByte));
}

} // namespace NEO
