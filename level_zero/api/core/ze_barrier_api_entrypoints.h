/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendBarrier>(hCommandList, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, false);
}

ze_result_t zeCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList,
    uint32_t numRanges,
    const size_t *pRangeSizes,
    const void **pRanges,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemoryRangesBarrier>(hCommandList, numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return L0::CommandList::fromHandle(hCommandList)->appendMemoryRangesBarrier(numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeDeviceSystemBarrier(
    ze_device_handle_t hDevice) {
    return L0::Device::fromHandle(hDevice)->systemBarrier();
}

ze_result_t ZE_APICALL zeCommandListHostSynchronize(
    ze_command_list_handle_t hCommandList,
    uint64_t timeout) {
    return L0::CommandList::fromHandle(hCommandList)->hostSynchronize(timeout);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendBarrier(
        hCommandList,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList,
    uint32_t numRanges,
    const size_t *pRangeSizes,
    const void **pRanges,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendMemoryRangesBarrier(
        hCommandList,
        numRanges,
        pRangeSizes,
        pRanges,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeDeviceSystemBarrier(
    ze_device_handle_t hDevice) {
    return L0::zeDeviceSystemBarrier(
        hDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListHostSynchronize(
    ze_command_list_handle_t hCommandList,
    uint64_t timeout) {
    return L0::zeCommandListHostSynchronize(
        hCommandList,
        timeout);
}
}
