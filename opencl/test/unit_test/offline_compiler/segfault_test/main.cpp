/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/signal_utils.h"

#include "gtest/gtest.h"
#include "segfault_helper.h"

#include <string>

StreamCapture capture;
extern int generateSegfaultWithSafetyGuard(SegfaultHelper *segfaultHelper);

int main(int argc, char **argv) {
    int retVal = 0;
    bool enableAlarm = true;

    ::testing::InitGoogleTest(&argc, argv);
    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_alarm", argv[i])) {
            enableAlarm = false;
        }
    }

    int sigOut = setAlarm(enableAlarm);
    if (sigOut != 0)
        return sigOut;

    retVal = RUN_ALL_TESTS();
    cleanupSignals();

    return retVal;
}

void captureAndCheckStdOut() {
    std::string callstack = capture.getCapturedStdout();

    EXPECT_TRUE(hasSubstr(callstack, std::string("Callstack")));
    EXPECT_TRUE(hasSubstr(callstack, std::string("cloc_segfault_test")));
    EXPECT_TRUE(hasSubstr(callstack, std::string("generateSegfaultWithSafetyGuard"))) << callstack;
}

TEST(SegFault, givenCallWithSafetyGuardWhenSegfaultHappensThenCallstackIsPrintedToStdOut) {
#if !defined(SKIP_SEGFAULT_TEST)
    capture.captureStdout();
    SegfaultHelper segfault;
    segfault.segfaultHandlerCallback = captureAndCheckStdOut;

    auto retVal = generateSegfaultWithSafetyGuard(&segfault);
    EXPECT_EQ(-60, retVal);
#else
    GTEST_SKIP();
#endif
}
