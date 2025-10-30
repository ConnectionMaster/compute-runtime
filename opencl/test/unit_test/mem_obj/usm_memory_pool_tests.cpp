/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "gtest/gtest.h"

using namespace NEO;

struct UsmPoolTest : public ::testing::Test {
    void SetUp() override {
        deviceFactory = std::make_unique<UltClDeviceFactoryWithPlatform>(2, 0);
        device = deviceFactory->rootDevices[0];
        if (device->deviceInfo.svmCapabilities == 0) {
            GTEST_SKIP();
        }
        cl_device_id deviceId = device;

        cl_int retVal;
        mockContext.reset(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        mockDeviceMemPoolsFacade = static_cast<MockUsmMemAllocPoolsFacade *>(&mockContext->getDeviceMemAllocPoolsManager());
    }

    MockClDevice *device;
    MockUsmMemAllocPoolsFacade *mockDeviceMemPoolsFacade;
    std::unique_ptr<UltClDeviceFactoryWithPlatform> deviceFactory;
    std::unique_ptr<MockContext> mockContext;
    constexpr static auto poolAllocationThreshold = MemoryConstants::pageSize;
};

TEST_F(UsmPoolTest, givenCreatedContextWhenCheckingUsmPoolsThenPoolsAreNotInitialized) {
    EXPECT_FALSE(mockDeviceMemPoolsFacade->isInitialized());
    EXPECT_EQ(nullptr, mockDeviceMemPoolsFacade->poolManager);
    EXPECT_EQ(nullptr, mockDeviceMemPoolsFacade->pool);

    MockUsmMemAllocPool *mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&mockContext->getDevice(0u)->getPlatform()->getHostMemAllocPool());
    EXPECT_FALSE(mockHostUsmMemAllocPool->isInitialized());
    EXPECT_EQ(0u, mockHostUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_EQ(nullptr, mockHostUsmMemAllocPool->pool);
}

TEST_F(UsmPoolTest, givenEnabledDebugFlagsAndUsmPoolsNotSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = false;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = false;

    auto platform = static_cast<MockPlatform *>(device->getPlatform());
    for (auto enablePoolManager : {false, true}) {
        debugManager.flags.EnableUsmAllocationPoolManager.set(enablePoolManager);
        platform->getHostMemAllocPool().cleanup();
        platform->usmPoolInitialized = false;
        mockContext->getDeviceMemAllocPoolsManager().cleanup();
        mockContext->usmPoolInitialized = false;

        cl_int retVal = CL_SUCCESS;
        void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, pooledDeviceAlloc);
        clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

        void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, pooledHostAlloc);
        clMemFreeINTEL(mockContext.get(), pooledHostAlloc);

        if (enablePoolManager) {
            EXPECT_NE(nullptr, mockDeviceMemPoolsFacade->poolManager);

            EXPECT_TRUE(platform->getHostMemAllocPool().isInitialized());
            EXPECT_TRUE(mockDeviceMemPoolsFacade->isInitialized());
        } else {
            EXPECT_NE(nullptr, mockDeviceMemPoolsFacade->pool);

            EXPECT_TRUE(platform->getHostMemAllocPool().isInitialized());
            EXPECT_TRUE(mockDeviceMemPoolsFacade->isInitialized());

            MockUsmMemAllocPool *mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(mockDeviceMemPoolsFacade->pool.get());
            EXPECT_EQ(1 * MemoryConstants::megaByte, mockDeviceUsmMemAllocPool->poolInfo.poolSize);
            EXPECT_NE(nullptr, mockDeviceUsmMemAllocPool->pool);
            EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, mockDeviceUsmMemAllocPool->poolMemoryType);

            MockUsmMemAllocPool *mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&platform->getHostMemAllocPool());
            EXPECT_EQ(3 * MemoryConstants::megaByte, mockHostUsmMemAllocPool->poolInfo.poolSize);
            EXPECT_NE(nullptr, mockHostUsmMemAllocPool->pool);
            EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, mockHostUsmMemAllocPool->poolMemoryType);
        }
    }
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedWhenCreatingAllocationsThenPoolsAreInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmAllocationPoolManager.set(0);

    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    cl_int retVal = CL_SUCCESS;
    void *pooledDeviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledDeviceAlloc);
    clMemFreeINTEL(mockContext.get(), pooledDeviceAlloc);

    void *pooledHostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pooledHostAlloc);
    clMemFreeINTEL(mockContext.get(), pooledHostAlloc);

    MockUsmMemAllocPool *mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(mockDeviceMemPoolsFacade->pool.get());
    MockUsmMemAllocPool *mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&device->getPlatform()->getHostMemAllocPool());
    EXPECT_EQ(UsmPoolParams::getUsmPoolSize(deviceFactory->rootDevices[0]->getGfxCoreHelper()), mockDeviceUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_EQ(UsmPoolParams::getUsmPoolSize(deviceFactory->rootDevices[0]->getGfxCoreHelper()), mockHostUsmMemAllocPool->poolInfo.poolSize);
    EXPECT_TRUE(mockDeviceUsmMemAllocPool->isInitialized());
    EXPECT_TRUE(mockHostUsmMemAllocPool->isInitialized());
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedAndDisabledByDebugFlagsWhenCreatingAllocationsThenPoolsAreNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(0);
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    cl_int retVal = CL_SUCCESS;
    void *deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), deviceAlloc);

    void *hostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clMemFreeINTEL(mockContext.get(), hostAlloc);

    EXPECT_FALSE(mockContext->getDeviceMemAllocPoolsManager().isInitialized());
    EXPECT_FALSE(mockContext->getDevice(0u)->getPlatform()->getHostMemAllocPool().isInitialized());
}

TEST_F(UsmPoolTest, givenUsmPoolsSupportedAndMultiDeviceContextWhenCreatingAllocationsThenDevicePoolIsNotInitialized) {
    RAIIProductHelperFactory<MockProductHelper> device1Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    RAIIProductHelperFactory<MockProductHelper> device2Raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[1]);
    device1Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device1Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;
    device2Raii.mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;

    auto platform = static_cast<MockPlatform *>(device->getPlatform());
    for (auto enablePoolManager : {false, true}) {
        DebugManagerStateRestore restorer;
        debugManager.flags.EnableUsmAllocationPoolManager.set(enablePoolManager);
        platform->getHostMemAllocPool().cleanup();
        platform->usmPoolInitialized = false;
        mockContext->getDeviceMemAllocPoolsManager().cleanup();
        mockContext->usmPoolInitialized = false;

        mockContext->devices.push_back(deviceFactory->rootDevices[1]);
        EXPECT_FALSE(mockContext->isSingleDeviceContext());

        cl_int retVal = CL_SUCCESS;
        void *deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, poolAllocationThreshold, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        clMemFreeINTEL(mockContext.get(), deviceAlloc);

        void *hostAlloc = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        clMemFreeINTEL(mockContext.get(), hostAlloc);

        EXPECT_TRUE(platform->getHostMemAllocPool().isInitialized());
        EXPECT_FALSE(mockContext->getDeviceMemAllocPoolsManager().isInitialized());

        mockContext->devices.pop_back();
    }
}

TEST_F(UsmPoolTest, givenTwoContextsWhenHostAllocationIsFreedInFirstContextThenItIsReusedInSecondContext) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmAllocationPoolManager.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(3);

    auto platform = static_cast<MockPlatform *>(device->getPlatform());
    for (auto enablePoolManager : {false, true}) {
        debugManager.flags.EnableUsmAllocationPoolManager.set(enablePoolManager);
        platform->getHostMemAllocPool().cleanup();
        platform->usmPoolInitialized = false;

        cl_int retVal = CL_SUCCESS;
        void *pooledHostAlloc1 = clHostMemAllocINTEL(mockContext.get(), nullptr, poolAllocationThreshold, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, pooledHostAlloc1);

        MockUsmMemAllocPool *mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&platform->getHostMemAllocPool());
        EXPECT_GT(mockHostUsmMemAllocPool->poolInfo.poolSize, 0u);

        clMemFreeINTEL(mockContext.get(), pooledHostAlloc1);

        cl_device_id deviceId = device;
        auto mockContext2 = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);

        void *pooledHostAlloc2 = clHostMemAllocINTEL(mockContext2.get(), nullptr, poolAllocationThreshold, 0, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, pooledHostAlloc2);

        EXPECT_EQ(pooledHostAlloc1, pooledHostAlloc2);

        clMemFreeINTEL(mockContext2.get(), pooledHostAlloc2);
    }
}

using UsmPoolManagerTest = UsmPoolTest;

TEST_F(UsmPoolManagerTest, givenCreatedContextWhenCheckingUsmPoolsManagersThenPoolsManagersAreNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmAllocationPoolManager.set(3);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);

    EXPECT_FALSE(mockContext->getDeviceMemAllocPoolsManager().isInitialized());
    EXPECT_EQ(nullptr, mockDeviceMemPoolsFacade->poolManager);
}

TEST_F(UsmPoolManagerTest, givenUsmPoolsManagerSupportedWhenCreatingAllocationsThenPoolFromManagerIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);

    cl_int retVal = CL_SUCCESS;
    auto deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, 1 * MemoryConstants::kiloByte, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockDeviceMemPoolsFacade->isInitialized());
    EXPECT_TRUE(mockDeviceMemPoolsFacade->poolManager->isInitialized());

    auto pool = mockDeviceMemPoolsFacade->getPoolContainingAlloc(deviceAlloc);
    EXPECT_NE(pool, nullptr);
    EXPECT_TRUE(pool->isInPool(deviceAlloc));

    clMemFreeINTEL(mockContext.get(), deviceAlloc);
}

TEST_F(UsmPoolManagerTest, givenUsmPoolsManagerAllocationsWhenFreeingAllocationsThenFreeIsSuccessful) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);

    cl_int retVal = CL_SUCCESS;
    auto deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, 1 * MemoryConstants::kiloByte, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockDeviceMemPoolsFacade->isInitialized());
    EXPECT_TRUE(mockDeviceMemPoolsFacade->poolManager->isInitialized());

    auto pool = mockDeviceMemPoolsFacade->getPoolContainingAlloc(deviceAlloc);
    EXPECT_NE(pool, nullptr);
    EXPECT_FALSE(pool->isEmpty());

    clMemFreeINTEL(mockContext.get(), deviceAlloc);
    EXPECT_TRUE(pool->isEmpty());
}

TEST_F(UsmPoolManagerTest, givenUsmPoolsManagerAllocationsWhenFreeingInvalidAllocationThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostUsmAllocationPool.set(3);
    debugManager.flags.EnableDeviceUsmAllocationPool.set(1);

    cl_int retVal = CL_SUCCESS;

    // first make valid allocation to initialize the pool manager
    auto deviceAlloc = clDeviceMemAllocINTEL(mockContext.get(), static_cast<cl_device_id>(mockContext->getDevice(0)), nullptr, 1 * MemoryConstants::kiloByte, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockDeviceMemPoolsFacade->isInitialized());
    EXPECT_TRUE(mockDeviceMemPoolsFacade->poolManager->isInitialized());
    EXPECT_NE(nullptr, mockDeviceMemPoolsFacade->getPoolContainingAlloc(deviceAlloc));
    retVal = clMemFreeINTEL(mockContext.get(), deviceAlloc);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clMemFreeINTEL(mockContext.get(), reinterpret_cast<void *>(0x123));
    EXPECT_NE(retVal, CL_SUCCESS);
}
