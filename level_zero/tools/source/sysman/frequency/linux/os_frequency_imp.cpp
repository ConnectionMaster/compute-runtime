/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/linux/os_frequency_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const bool LinuxFrequencyImp::canControl = true; // canControl is true on i915 (GEN9 Hardcode)

ze_result_t LinuxFrequencyImp::osFrequencyGetProperties(zes_freq_properties_t &properties) {
    properties.pNext = nullptr;
    properties.canControl = canControl;
    ze_result_t result1 = getMinVal(properties.min);
    ze_result_t result2 = getMaxVal(properties.max);
    // If can't figure out the valid range, then can't control it.
    if (ZE_RESULT_SUCCESS != result1 || ZE_RESULT_SUCCESS != result2) {
        properties.canControl = false;
        properties.min = 0.0;
        properties.max = 0.0;
    }
    properties.isThrottleEventSupported = false;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::osFrequencyGetRange(zes_freq_range_t *pLimits) {
    ze_result_t result = getMax(pLimits->max);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return getMin(pLimits->min);
}

ze_result_t LinuxFrequencyImp::osFrequencySetRange(const zes_freq_range_t *pLimits) {
    double newMin = round(pLimits->min);
    double newMax = round(pLimits->max);
    double currentMax = 0.0;
    ze_result_t result = getMax(currentMax);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    if (newMin > currentMax) {
        // set the max first
        ze_result_t result = setMax(newMax);
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }
        return setMin(newMin);
    }

    // set the min first
    result = setMin(newMin);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    return setMax(newMax);
}

ze_result_t LinuxFrequencyImp::osFrequencyGetState(zes_freq_state_t *pState) {
    ze_result_t result;

    result = getRequest(pState->request);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = getTdp(pState->tdp);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = getEfficient(pState->efficient);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = getActual(pState->actual);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    pState->pNext = nullptr;
    pState->currentVoltage = -1.0;
    pState->throttleReasons = 0u;
    return result;
}

ze_result_t LinuxFrequencyImp::osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFrequencyImp::getMin(double &min) {
    double intval;

    ze_result_t result = pSysfsAccess->read(minFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    min = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::setMin(double min) {
    ze_result_t result = pSysfsAccess->write(minFreqFile, min);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMax(double &max) {
    double intval;

    ze_result_t result = pSysfsAccess->read(maxFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    max = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::setMax(double max) {
    ze_result_t result = pSysfsAccess->write(maxFreqFile, max);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getRequest(double &request) {
    double intval;

    ze_result_t result = pSysfsAccess->read(requestFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    request = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getTdp(double &tdp) {
    double intval;

    ze_result_t result = pSysfsAccess->read(tdpFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    tdp = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getActual(double &actual) {
    double intval;

    ze_result_t result = pSysfsAccess->read(actualFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    actual = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getEfficient(double &efficient) {
    double intval;

    ze_result_t result = pSysfsAccess->read(efficientFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    efficient = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMaxVal(double &maxVal) {
    double intval;

    ze_result_t result = pSysfsAccess->read(maxValFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    maxVal = intval;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFrequencyImp::getMinVal(double &minVal) {
    double intval;

    ze_result_t result = pSysfsAccess->read(minValFreqFile, intval);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    minVal = intval;
    return ZE_RESULT_SUCCESS;
}

void LinuxFrequencyImp::init() {
    minFreqFile = "gt_min_freq_mhz";
    maxFreqFile = "gt_max_freq_mhz";
    requestFreqFile = "gt_cur_freq_mhz";
    tdpFreqFile = "gt_boost_freq_mhz";
    actualFreqFile = "gt_act_freq_mhz";
    efficientFreqFile = "gt_RP1_freq_mhz";
    maxValFreqFile = "gt_RP0_freq_mhz";
    minValFreqFile = "gt_RPn_freq_mhz";
}

LinuxFrequencyImp::LinuxFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    init();
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxFrequencyImp *pLinuxFrequencyImp = new LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsFrequency *>(pLinuxFrequencyImp);
}

uint16_t OsFrequency::getHardwareBlockCount(ze_device_handle_t handle) {
    return 1; // hardcode for now to support only ZES_FREQ_DOMAIN_GPU
}

} // namespace L0
