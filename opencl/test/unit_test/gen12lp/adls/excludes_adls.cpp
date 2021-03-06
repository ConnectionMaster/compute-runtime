/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

namespace NEO {
HWCMDTEST_EXCLUDE_FAMILY(ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingTests, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(EventProfilingTest, givenEventWhenCompleteIsZeroThenCalcProfilingDataSetsEndTimestampInCompleteTimestampAndDoesntCallOsTimeMethods, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(EventProfilingTests, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturned, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(EventProfilingTest, givenRawTimestampsDebugModeWhenStartTimeStampLTQueueTimeStampThenIncreaseStartTimeStamp, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenRegisterStoresArePresentInCS, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_ALDERLAKE_S);
} // namespace NEO

HWCMDTEST_EXCLUDE_FAMILY(DeviceFactoryTest, givenInvalidHwConfigStringWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenThrowsException, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsTakenThenTimeStampAddressIsProgrammedCorrectly, IGFX_ALDERLAKE_S);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsNotTakenThenTimeStampAddressIsProgrammedCorrectly, IGFX_ALDERLAKE_S);
