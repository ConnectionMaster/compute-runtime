/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/windows/os_fan_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/fan/windows/mock_fan.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t fanHandleComponentCount = 1u;
class SysmanDeviceFanFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<FanKmdSysManager>> pKmdSysManager;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls, bool fanSupported) {
        pKmdSysManager.reset(new Mock<FanKmdSysManager>);

        pKmdSysManager->allowSetCalls = allowSetCalls;
        pKmdSysManager->fanSupported = fanSupported;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pFanHandleContext->handleList.clear();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_fan_handle_t> getFanHandles() {
        uint32_t count = 0;
        EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
        std::vector<zes_fan_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFanFixture, GivenComponentCountZeroWhenEnumeratingFansThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true, true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);
}

TEST_F(SysmanDeviceFanFixture, GivenInvalidComponentCountWhenEnumeratingFansThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true, true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);
}

TEST_F(SysmanDeviceFanFixture, GivenComponentCountZeroWhenEnumeratingFansThenValidFanHandlesIsReturned) {
    init(true, true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);

    std::vector<zes_fan_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanPropertiesAllowSetToTrueThenCallSucceeds) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties;

        ze_result_t result = zesFanGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(properties.canControl);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxPoints), pKmdSysManager->mockFanMaxPoints);
        EXPECT_EQ(properties.maxRPM, -1);
        EXPECT_EQ(properties.supportedModes, zes_fan_speed_mode_t::ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(properties.supportedUnits, zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanPropertiesAllowSetToFalseThenControlToFalse) {
    // Setting allow set calls or not
    init(false, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties = {};
        ze_result_t result = zesFanGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.canControl);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidNoSupportForFanCheckFanHandleCountIsZero) {
    // Setting allow set calls or not
    init(false, false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanPropertiesAllowSetToFalseThenCallSucceeds) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties;

        ze_result_t result = zesFanGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(properties.canControl);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxPoints), pKmdSysManager->mockFanMaxPoints);
        EXPECT_EQ(properties.maxRPM, -1);
        EXPECT_EQ(properties.supportedModes, zes_fan_speed_mode_t::ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(properties.supportedUnits, zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigThenSuccessIsReturnedDefaultFanTable) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigThenFirstSingleRequestFails) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    pKmdSysManager->mockRequestSingle = TRUE;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesFanGetConfig(handle, &fanConfig));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigWithValidFanPointsSuccessCustomFanTable) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 4;
        fanSpeedTable.table[0].speed.speed = 65;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 75;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 45;

        fanSpeedTable.table[2].speed.speed = 85;
        fanSpeedTable.table[2].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[2].temperature = 60;

        fanSpeedTable.table[3].speed.speed = 100;
        fanSpeedTable.table[3].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[3].temperature = 90;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetSpeedTableMode(handle, &fanSpeedTable));

        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 4);
        EXPECT_EQ(fanConfig.speedTable.table[0].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[1].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[2].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[3].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingDefaultModeThenSupportedIsReturned) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingFixedSpeedModeThenUnsupportedIsReturned) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_t fanSpeed = {0};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetFixedSpeedMode(handle, &fanSpeed));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingTheSpeedTableModeWithNumberOfPointsZeroThenUnsupportedIsReturned) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingTheSpeedTableModeWithGreaterThanMaxNumberOfPointsThenUnsupportedIsReturned) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 20; // Setting number of control points greater than max number of control points (10)
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithRPMUnitThenValidFanSpeedReadingsRetrieved) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_GT(fanSpeed, 0);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithPercentUnitThenUnsupportedIsReturned) {
    // Setting allow set calls or not
    init(true, true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;
        int32_t fanSpeed = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(handle, unit, &fanSpeed));
    }
}

} // namespace ult
} // namespace L0
