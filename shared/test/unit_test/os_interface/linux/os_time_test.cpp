/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <dlfcn.h>

static uint64_t actualTime = 0;

int getTimeFuncFalse(clockid_t clkId, struct timespec *tp) throw() {
    return -1;
}
int getTimeFuncTrue(clockid_t clkId, struct timespec *tp) throw() {
    tp->tv_sec = 0;
    tp->tv_nsec = static_cast<long>(++actualTime);
    return 0;
}

int resolutionFuncFalse(clockid_t clkId, struct timespec *res) throw() {
    return -1;
}
int resolutionFuncTrue(clockid_t clkId, struct timespec *res) throw() {
    res->tv_sec = 0;
    res->tv_nsec = 5;
    return 0;
}

using namespace NEO;
struct DrmTimeTest : public ::testing::Test {
  public:
    void SetUp() override {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
        osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
        osTime->setResolutionFunc(resolutionFuncTrue);
        osTime->setGetTimeFunc(getTimeFuncTrue);
        auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
        osTime->setDeviceTimerResolution(*hwInfo);
        deviceTime = osTime->getDeviceTime();
    }

    void TearDown() override {
    }
    MockDeviceTimeDrm *deviceTime = nullptr;
    std::unique_ptr<MockOSTimeLinux> osTime;
    MockExecutionEnvironment executionEnvironment;
};

TEST_F(DrmTimeTest, WhenGettingCpuTimeThenSucceeds) {
    uint64_t time = 0;
    auto error = osTime->getCpuTime(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
}

TEST_F(DrmTimeTest, GivenFalseTimeFuncWhenGettingCpuTimeThenFails) {
    uint64_t time = 0;
    osTime->setGetTimeFunc(getTimeFuncFalse);
    auto error = osTime->getCpuTime(&time);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, WhenGettingGpuCpuTimeThenSucceeds) {
    TimeStampData gpuCpuTime01 = {0, 0};
    TimeStampData gpuCpuTime02 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime01.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime01.gpuTimeStamp);
    error = osTime->getGpuCpuTime(&gpuCpuTime02);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime02.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime02.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.gpuTimeStamp, gpuCpuTime01.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.cpuTimeinNS, gpuCpuTime01.cpuTimeinNS);
}

TEST_F(DrmTimeTest, GivenDrmWhenGettingGpuCpuTimeThenSucceeds) {
    TimeStampData gpuCpuTime01 = {0, 0};
    TimeStampData gpuCpuTime02 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime01.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime01.gpuTimeStamp);
    error = osTime->getGpuCpuTime(&gpuCpuTime02);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime02.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime02.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.gpuTimeStamp, gpuCpuTime01.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.cpuTimeinNS, gpuCpuTime01.cpuTimeinNS);
}

TEST_F(DrmTimeTest, givenGetGpuCpuTimeWhenItIsUnavailableThenReturnFalse) {
    TimeStampData gpuCpuTime = {0, 0};

    deviceTime->callBaseGetGpuCpuTimeImpl = false;
    deviceTime->getGpuCpuTimeImplResult = false;
    auto error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, given36BitGpuTimeStampWhenGpuTimeStampOverflowThenGpuTimeDoesNotDecrease) {
    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->callBaseGetGpuCpuTimeImpl = false;
    deviceTime->gpuCpuTimeValue = {100ull, 100ull};
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(100ull, gpuCpuTime.gpuTimeStamp);

    actualTime = 200ll;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(actualTime, gpuCpuTime.gpuTimeStamp);

    osTime->maxGpuTimeStamp = 1ull << 36;

    deviceTime->gpuCpuTimeValue = {10ull, 10ull}; // read from KMD below initial value
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime, true));
    EXPECT_EQ(osTime->maxGpuTimeStamp + 10ull, gpuCpuTime.gpuTimeStamp);

    actualTime = 30ull; // second read below initial value
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(osTime->maxGpuTimeStamp + actualTime, gpuCpuTime.gpuTimeStamp);

    actualTime = 110ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(osTime->maxGpuTimeStamp + actualTime, gpuCpuTime.gpuTimeStamp);

    actualTime = 70ull; // second overflow
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(2ull * osTime->maxGpuTimeStamp + actualTime, gpuCpuTime.gpuTimeStamp);
}

TEST_F(DrmTimeTest, given64BitGpuTimeStampWhenGpuTimeStampOverflowThenOverflowsAreNotDetected) {
    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->callBaseGetGpuCpuTimeImpl = false;
    deviceTime->gpuCpuTimeValue = {100ull, 100ull};
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(100ull, gpuCpuTime.gpuTimeStamp);

    actualTime = 200ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(actualTime, gpuCpuTime.gpuTimeStamp);

    osTime->maxGpuTimeStamp = 0ull;

    deviceTime->gpuCpuTimeValue = {10ull, 10ull}; // read from KMD below initial value
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime, true));
    EXPECT_EQ(10ull, gpuCpuTime.gpuTimeStamp);

    actualTime = 30ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(actualTime, gpuCpuTime.gpuTimeStamp);

    actualTime = 110ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(actualTime, gpuCpuTime.gpuTimeStamp);

    actualTime = 70ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(actualTime, gpuCpuTime.gpuTimeStamp);
}

TEST_F(DrmTimeTest, GivenInvalidDrmWhenGettingGpuCpuTimeThenFails) {
    TimeStampData gpuCpuTime01 = {0, 0};
    auto pDrm = new DrmMockFail(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, GivenInvalidFuncTimeWhenGettingGpuCpuTimeCpuThenFails) {
    TimeStampData gpuCpuTime01 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->setGetTimeFunc(getTimeFuncFalse);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenIoctlFailsThenDefaultResolutionIsReturned) {
    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    drm->getParamRetValue = 0;
    drm->ioctlRes = -1;
    deviceTime->callGetDynamicDeviceTimerResolution = true;
    auto result = osTime->getDynamicDeviceTimerResolution(*defaultHwInfo);
    EXPECT_DOUBLE_EQ(result, defaultResolution);
}

TEST_F(DrmTimeTest, givenGetDynamicDeviceTimerClockWhenIoctlFailsThenDefaultClockIsReturned) {
    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    drm->getParamRetValue = 0;
    drm->ioctlRes = -1;

    auto result = osTime->getDynamicDeviceTimerClock(*defaultHwInfo);
    auto expectedResult = static_cast<uint64_t>(1000000000.0 / defaultResolution);
    EXPECT_EQ(result, expectedResult);
}

TEST_F(DrmTimeTest, givenGetDynamicDeviceTimerClockWhenIoctlSucceedsThenNonDefaultClockIsReturned) {
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    uint64_t frequency = 1500;
    drm->getParamRetValue = static_cast<int>(frequency);

    auto result = osTime->getDynamicDeviceTimerClock(*defaultHwInfo);
    EXPECT_EQ(result, frequency);
}

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenIoctlSuccedsThenCorrectResolutionIsReturned) {
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    // 19200000 is frequency yelding 52.083ns resolution
    drm->getParamRetValue = 19200000;
    drm->ioctlRes = 0;
    deviceTime->callGetDynamicDeviceTimerResolution = true;
    auto result = osTime->getDynamicDeviceTimerResolution(*defaultHwInfo);
    EXPECT_DOUBLE_EQ(result, 52.08333333333333);
}

TEST_F(DrmTimeTest, givenAlwaysFailingResolutionFuncWhenGetHostTimerResolutionIsCalledThenReturnsZero) {
    osTime->setResolutionFunc(resolutionFuncFalse);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(0, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysPassingResolutionFuncWhenGetHostTimerResolutionIsCalledThenReturnsNonzero) {
    osTime->setResolutionFunc(resolutionFuncTrue);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(5, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysFailingResolutionFuncWhenGetCpuRawTimestampIsCalledThenReturnsZero) {
    osTime->setResolutionFunc(resolutionFuncFalse);
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_EQ(0ull, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysFailingGetTimeFuncWhenGetCpuRawTimestampIsCalledThenReturnsZero) {
    osTime->setGetTimeFunc(getTimeFuncFalse);
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_EQ(0ull, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysPassingResolutionFuncWhenGetCpuRawTimestampIsCalledThenReturnsNonzero) {
    actualTime = 4;
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_EQ(1ull, retVal);
}

TEST_F(DrmTimeTest, whenGettingMaxGpuTimeStampValueThenHwInfoBasedValueIsReturned) {
    if (defaultHwInfo->capabilityTable.timestampValidBits < 64) {
        auto expectedMaxGpuTimeStampValue = 1ull << defaultHwInfo->capabilityTable.timestampValidBits;
        EXPECT_EQ(expectedMaxGpuTimeStampValue, osTime->getMaxGpuTimeStamp());
    } else {
        EXPECT_EQ(0ull, osTime->getMaxGpuTimeStamp());
    }
}

TEST_F(DrmTimeTest, whenGettingGpuTimeStampValueWithinIntervalThenReuseFromPreviousCall) {
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();

    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);
    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    auto gpuTimestampBefore = gpuCpuTime.gpuTimeStamp;
    auto cpuTimeBefore = actualTime;

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    auto gpuTimestampAfter = gpuCpuTime.gpuTimeStamp;
    auto cpuTimeAfter = actualTime;

    auto cpuTimeDiff = cpuTimeAfter - cpuTimeBefore;
    auto deviceTimerResolution = deviceTime->getDynamicDeviceTimerResolution(*hwInfo);
    auto gpuTimestampDiff = static_cast<uint64_t>(cpuTimeDiff / deviceTimerResolution);
    EXPECT_EQ(gpuTimestampAfter, gpuTimestampBefore + gpuTimestampDiff);
}

TEST_F(DrmTimeTest, whenGettingGpuTimeStampValueAfterIntervalThenCallToKmdAndAdaptTimeout) {
    deviceTime->callBaseGetGpuCpuTimeImpl = false;
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);

    const auto initialExpectedTimeoutNS = NSEC_PER_MSEC * 100;
    EXPECT_EQ(initialExpectedTimeoutNS, osTime->getTimestampRefreshTimeout());

    auto setTimestamps = [&](uint64_t cpuTimeNS, uint64_t cpuTimeFromKmdNS, uint64_t gpuTimestamp) {
        actualTime = cpuTimeNS;
        deviceTime->gpuCpuTimeValue.cpuTimeinNS = cpuTimeFromKmdNS;
        deviceTime->gpuCpuTimeValue.gpuTimeStamp = gpuTimestamp;
    };
    setTimestamps(0, 0ull, 0ull);

    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    // Error is smaller than 5%, timeout can be increased
    auto newTimeAfterInterval = actualTime + osTime->getTimestampRefreshTimeout();
    setTimestamps(newTimeAfterInterval, newTimeAfterInterval + 10, newTimeAfterInterval + 10);

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);

    auto diff = (gpuCpuTime.gpuTimeStamp - actualTime);
    EXPECT_EQ(initialExpectedTimeoutNS + diff, osTime->getTimestampRefreshTimeout());
    EXPECT_GT(initialExpectedTimeoutNS + diff, initialExpectedTimeoutNS);

    // Error is larger than 5%, timeout should be decreased
    newTimeAfterInterval = actualTime + osTime->getTimestampRefreshTimeout() + 10;
    setTimestamps(newTimeAfterInterval, newTimeAfterInterval * 2, newTimeAfterInterval * 2);

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 3u);

    EXPECT_LT(osTime->getTimestampRefreshTimeout(), initialExpectedTimeoutNS);
}

TEST_F(DrmTimeTest, whenGettingMaxGpuTimeStampValueAfterFlagSetThenCallToKmd) {
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);
    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    osTime->setRefreshTimestampsFlag();
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);
}

TEST_F(DrmTimeTest, whenGettingMaxGpuTimeStampValueWhenForceFlagSetThenCallToKmd) {
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);
    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    osTime->getGpuCpuTime(&gpuCpuTime, true);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);
}

TEST_F(DrmTimeTest, givenReusingTimestampsDisabledWhenGetTimestampRefreshTimeoutThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableReusingGpuTimestamps.set(0);
    // Recreate mock to apply debug flag
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    osTime->setResolutionFunc(resolutionFuncTrue);
    osTime->setGetTimeFunc(getTimeFuncTrue);
    EXPECT_EQ(0ul, osTime->getTimestampRefreshTimeout());
}

TEST_F(DrmTimeTest, givenReusingTimestampsDisabledWhenGetGpuCpuTimeThenAlwaysCallKmd) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableReusingGpuTimestamps.set(0);
    // Recreate mock to apply debug flag
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    osTime->setResolutionFunc(resolutionFuncTrue);
    osTime->setGetTimeFunc(getTimeFuncTrue);
    auto deviceTime = osTime->getDeviceTime();
    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);
}
