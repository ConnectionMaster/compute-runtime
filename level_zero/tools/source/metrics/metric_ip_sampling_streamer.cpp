/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"

#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include <level_zero/zet_api.h>

#include <set>
#include <string.h>

namespace L0 {

ze_result_t IpSamplingMetricGroupImp::streamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {

    auto device = Device::fromHandle(hDevice);

    // Check whether metric group is activated.
    IpSamplingMetricSourceImp &source = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    if (!source.isMetricGroupActivated(this->toHandle())) {
        return ZE_RESULT_NOT_READY;
    }

    // Check whether metric streamer is already open.
    if (source.pActiveStreamer != nullptr) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    auto pStreamerImp = new IpSamplingMetricStreamerImp(source);
    UNRECOVERABLE_IF(pStreamerImp == nullptr);

    const ze_result_t result = source.getMetricOsInterface()->startMeasurement(desc->notifyEveryNReports, desc->samplingPeriod);
    if (result == ZE_RESULT_SUCCESS) {
        source.pActiveStreamer = pStreamerImp;
        pStreamerImp->attachEvent(hNotificationEvent);
    } else {
        delete pStreamerImp;
        pStreamerImp = nullptr;
        return result;
    }

    *phMetricStreamer = pStreamerImp->toHandle();

    auto hwBufferSizeDesc = MetricSource::getHwBufferSizeDesc(static_cast<zet_base_desc_t *>(const_cast<void *>(desc->pNext)));
    if (hwBufferSizeDesc.has_value()) {
        // EUSS uses fixed max hw buffer size
        hwBufferSizeDesc.value()->sizeInBytes = source.getMetricOsInterface()->getRequiredBufferSize(UINT32_MAX);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) {

    // Return required size if requested.
    if (*pRawDataSize == 0) {
        *pRawDataSize = ipSamplingSource.getMetricOsInterface()->getRequiredBufferSize(maxReportCount);
        return ZE_RESULT_SUCCESS;
    }

    // If there is a difference in pRawDataSize and maxReportCount, use the minimum value for reading.
    if (maxReportCount != UINT32_MAX) {
        size_t maxSizeRequired = ipSamplingSource.getMetricOsInterface()->getRequiredBufferSize(maxReportCount);
        *pRawDataSize = std::min(maxSizeRequired, *pRawDataSize);
    }

    return ipSamplingSource.getMetricOsInterface()->readData(pRawData, pRawDataSize);
}

ze_result_t IpSamplingMetricStreamerImp::close() {

    const ze_result_t result = ipSamplingSource.getMetricOsInterface()->stopMeasurement();
    detachEvent();
    ipSamplingSource.pActiveStreamer = nullptr;
    delete this;

    return result;
}

Event::State IpSamplingMetricStreamerImp::getNotificationState() {

    return ipSamplingSource.getMetricOsInterface()->isNReportsAvailable()
               ? Event::State::STATE_SIGNALED
               : Event::State::STATE_INITIAL;
}

uint32_t IpSamplingMetricStreamerImp::getMaxSupportedReportCount() {
    const auto unitReportSize = ipSamplingSource.getMetricOsInterface()->getUnitReportSize();
    UNRECOVERABLE_IF(unitReportSize == 0);
    return ipSamplingSource.getMetricOsInterface()->getRequiredBufferSize(UINT32_MAX) / unitReportSize;
}

ze_result_t IpSamplingMetricCalcOpImp::create(bool isMultiDevice,
                                              const std::vector<MetricScopeImp *> &metricScopes,
                                              IpSamplingMetricSourceImp &metricSource,
                                              zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                                              zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation) {

    // There is only one valid metric group in IP sampling and time filtering is not supported
    // So only metrics handles are used to filter the metrics

    // avoid duplicates
    std::set<zet_metric_handle_t> uniqueMetricHandles(pCalculationDesc->phMetrics, pCalculationDesc->phMetrics + pCalculationDesc->metricCount);

    // The order of metrics in the report should be the same as the one in the HW report to optimize calculation
    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t hMetricGroup = {};
    if (auto ret = metricSource.metricGroupGet(&metricGroupCount, &hMetricGroup); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }
    uint32_t metricCount = 0;
    MetricGroup::fromHandle(hMetricGroup)->metricGet(&metricCount, nullptr);
    std::vector<zet_metric_handle_t> hMetrics(metricCount);
    MetricGroup::fromHandle(hMetricGroup)->metricGet(&metricCount, hMetrics.data());
    std::vector<MetricImp *> metricsInReport = {};
    std::vector<uint32_t> includedMetricIndexes = {};

    for (uint32_t i = 0; i < metricCount; i++) {
        auto metric = static_cast<MetricImp *>(Metric::fromHandle(hMetrics[i]));
        if (pCalculationDesc->metricGroupCount > 0) {
            metricsInReport.push_back(metric);
            includedMetricIndexes.push_back(i);
        } else {
            if (uniqueMetricHandles.find(hMetrics[i]) != uniqueMetricHandles.end()) {
                metricsInReport.push_back(metric);
                includedMetricIndexes.push_back(i);
            }
        }
    }

    auto calcOp = new IpSamplingMetricCalcOpImp(isMultiDevice, metricScopes, metricsInReport,
                                                metricSource, includedMetricIndexes);

    *phCalculationOperation = calcOp->toHandle();
    ze_result_t status = ZE_RESULT_SUCCESS;
    if ((pCalculationDesc->timeWindowsCount > 0) || (pCalculationDesc->timeAggregationWindow != 0)) {
        // Time filtering is not supported in IP sampling
        status = ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP;
        METRICS_LOG_INFO("%s", "Time filtering is not supported in IP sampling, ignoring time windows and aggregation window");
    }

    return status;
}

ze_result_t IpSamplingMetricCalcOpImp::destroy() {
    for (auto &it : scopeIdToCache) {
        if (!isScopeCacheEmpty(it.first)) {
            clearScopeCache(it.first);
        }
        delete it.second;
    }
    scopeIdToCache.clear();
    delete this;
    return ZE_RESULT_SUCCESS;
}

void IpSamplingMetricCalcOpImp::clearScopeCache(uint32_t scopeId) {
    const auto deviceImp = static_cast<DeviceImp *>(&(metricSource.getMetricDeviceContext().getDevice()));
    L0::L0GfxCoreHelper &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (scopeIdToCache.find(scopeId) != scopeIdToCache.end()) {
        l0GfxCoreHelper.stallIpDataMapDelete(*scopeIdToCache[scopeId]);
        scopeIdToCache[scopeId]->clear();
    }
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValuesSingle(const size_t rawDataSize, const uint8_t *pRawData,
                                                                   bool newData, uint32_t *pTotalMetricReportCount,
                                                                   bool getSize, uint32_t scopeId,
                                                                   bool &dataOverflow) {
    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;

    auto ipSamplingCalculation = metricSource.ipSamplingCalculation.get();

    if (getSize) {
        std::unordered_set<uint64_t> iPs{};
        if (newData) {
            status = ipSamplingCalculation->getIpsInRawData(pRawData, rawDataSize, iPs);
            if (status != ZE_RESULT_SUCCESS) {
                *pTotalMetricReportCount = 0;
                return status;
            }
        }

        // Add cached IPs
        if (!isScopeCacheEmpty(scopeId)) {
            getScopeCacheIps(scopeId, iPs);
        }

        *pTotalMetricReportCount = static_cast<uint32_t>(iPs.size());

        DEBUG_BREAK_IF(*pTotalMetricReportCount == 0);

        return ZE_RESULT_SUCCESS;
    }

    if (newData) {
        ipSamplingCalculation->fillStallDataMap(rawDataSize, pRawData,
                                                *getScopeCache(scopeId), &dataOverflow);
    }

    DEBUG_BREAK_IF(isScopeCacheEmpty(scopeId));

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValuesMulti(const size_t rawDataSize, const uint8_t *pRawData,
                                                                  uint32_t *pTotalMetricReportCount,
                                                                  bool getSize,
                                                                  uint32_t numSubDevices,
                                                                  size_t *usedSize,
                                                                  bool &dataOverflow) {
    ze_result_t status = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    return status;
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValues(const size_t rawDataSize, const uint8_t *pRawData,
                                                             bool final, size_t *usedSize,
                                                             uint32_t *pTotalMetricReportCount,
                                                             zet_intel_metric_result_exp_t *pMetricResults) {
    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;
    bool dataOverflow = false;

    auto ipSamplingCalculation = metricSource.ipSamplingCalculation.get();
    bool isMultiDeviceData = IpSamplingCalculation::isMultiDeviceCaptureData(rawDataSize, pRawData);

    bool getSize = (*pTotalMetricReportCount == 0);

    bool newData = false; // Track if there is fresh new raw data that requires updating caches
    uint64_t dataSize = rawDataSize;
    const uint8_t *rawDataStart = pRawData;

    if (areAllCachesEmpty()) {
        // All data is new: user asked to calculate all results available in the raw data. So, all caches are empty
        newData = true;
    } else if (rawDataSize > processedSize) {
        // Previous call user requested fewer results than available. So, algo cached pending results and
        // processed size = input size - rawReportSize  because returned used size = rawReportSize.
        // Then user is expected to move pRawData by rawReportSize. If data gets appended user must update
        // new size accordingly.
        newData = true;
        rawDataStart += processedSize;
        dataSize = rawDataSize - processedSize;
    }

    if (!isMultiDevice) {
        if (isMultiDeviceData) {
            METRICS_LOG_ERR("%s", "Cannot use root device raw data in a sub-device calculation operation handle");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        uint32_t scopeId = 0; // Sub-devices will always have its compute scope with ID 0
        status = metricCalculateValuesSingle(dataSize, rawDataStart, newData, pTotalMetricReportCount,
                                             getSize, scopeId, dataOverflow);
        if ((status != ZE_RESULT_SUCCESS) || (getSize)) {
            return status;
        }

        bool keepCachedResults = (*pTotalMetricReportCount < static_cast<uint32_t>(getScopeCache(scopeId)->size()));

        uint32_t metricReportCount = std::min<uint32_t>(*pTotalMetricReportCount, static_cast<uint32_t>(getScopeCache(scopeId)->size()));
        ipSamplingCalculation->stallDataMapToMetricResults(*getScopeCache(scopeId), metricReportCount, includedMetricIndexes, pMetricResults);

        if (keepCachedResults) {
            *usedSize = IpSamplingCalculation::rawReportSize;
            processedSize = rawDataSize - IpSamplingCalculation::rawReportSize;
        } else {
            *usedSize = rawDataSize;
            processedSize = 0;
        }

        if (final || !keepCachedResults) {
            clearScopeCache(scopeId);
        }

    } else {
        if (!isMultiDeviceData) {
            METRICS_LOG_ERR("%s", "Cannot use sub-device raw data in a root device calculation operation handle");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        DeviceImp *deviceImp = static_cast<DeviceImp *>(&metricSource.getMetricDeviceContext().getDevice());
        status = metricCalculateValuesMulti(rawDataSize, pRawData, pTotalMetricReportCount,
                                            getSize, deviceImp->numSubDevices,
                                            usedSize, dataOverflow);
        return status;
    }

    return dataOverflow ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::streamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {

    ze_result_t result = ZE_RESULT_SUCCESS;

    std::vector<IpSamplingMetricStreamerImp *> subDeviceStreamers = {};
    const auto numSubDevices = subDeviceMetricGroup.size();
    subDeviceStreamers.reserve(numSubDevices);

    // Open SubDevice Streamers
    for (auto &metricGroup : subDeviceMetricGroup) {
        zet_metric_streamer_handle_t subDeviceMetricStreamer = {};
        zet_device_handle_t hSubDevice = metricGroup->getMetricSource().getMetricDeviceContext().getDevice().toHandle();
        result = metricGroup->streamerOpen(hContext, hSubDevice, desc, nullptr, &subDeviceMetricStreamer);
        if (result != ZE_RESULT_SUCCESS) {
            closeSubDeviceStreamers(subDeviceStreamers);
            return result;
        }
        subDeviceStreamers.push_back(static_cast<IpSamplingMetricStreamerImp *>(MetricStreamer::fromHandle(subDeviceMetricStreamer)));
    }

    auto pStreamerImp = new MultiDeviceIpSamplingMetricStreamerImp(subDeviceStreamers);
    UNRECOVERABLE_IF(pStreamerImp == nullptr);

    pStreamerImp->attachEvent(hNotificationEvent);
    *phMetricStreamer = pStreamerImp->toHandle();

    auto hwBufferSizeDesc = MetricSource::getHwBufferSizeDesc(static_cast<zet_base_desc_t *>(const_cast<void *>(desc->pNext)));
    if (hwBufferSizeDesc.has_value()) {
        auto device = Device::fromHandle(hDevice);
        IpSamplingMetricSourceImp &source = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        // EUSS uses fixed max hw buffer size
        hwBufferSizeDesc.value()->sizeInBytes = source.getMetricOsInterface()->getRequiredBufferSize(UINT32_MAX) * numSubDevices;
    }
    return result;
}

ze_result_t MultiDeviceIpSamplingMetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) {

    const int32_t totalHeaderSize = static_cast<int32_t>(sizeof(IpSamplingMetricDataHeader) * subDeviceStreamers.size());
    // Find single report size
    size_t singleReportSize = 0;
    subDeviceStreamers[0]->readData(1, &singleReportSize, nullptr);

    // Trim report count to the maximum possible report count
    const uint32_t maxSupportedReportCount = subDeviceStreamers[0]->getMaxSupportedReportCount() *
                                             static_cast<uint32_t>(subDeviceStreamers.size());
    maxReportCount = std::min(maxSupportedReportCount, maxReportCount);

    if (*pRawDataSize == 0) {
        *pRawDataSize = singleReportSize * maxReportCount;
        *pRawDataSize += totalHeaderSize;
        return ZE_RESULT_SUCCESS;
    }

    // Remove header size from actual data size
    size_t calcRawDataSize = std::max<int32_t>(0, static_cast<int32_t>(*pRawDataSize - totalHeaderSize));

    // Recalculate maximum possible report count for the raw data size
    calcRawDataSize = std::min(calcRawDataSize, singleReportSize * maxReportCount);
    maxReportCount = static_cast<uint32_t>(calcRawDataSize / singleReportSize);
    uint8_t *pCurrRawData = pRawData;
    size_t currRawDataSize = calcRawDataSize;

    ze_result_t result = ZE_RESULT_SUCCESS;

    for (uint32_t index = 0; index < subDeviceStreamers.size(); index++) {
        auto &streamer = subDeviceStreamers[index];

        // Get header address
        auto header = reinterpret_cast<IpSamplingMetricDataHeader *>(pCurrRawData);
        pCurrRawData += sizeof(IpSamplingMetricDataHeader);

        result = streamer->readData(maxReportCount, &currRawDataSize, pCurrRawData);
        if (result != ZE_RESULT_SUCCESS) {
            *pRawDataSize = 0;
            return result;
        }

        // Update to header
        memset(header, 0, sizeof(IpSamplingMetricDataHeader));
        header->magic = IpSamplingMetricDataHeader::magicValue;
        header->rawDataSize = static_cast<uint32_t>(currRawDataSize);
        header->setIndex = index;

        calcRawDataSize -= currRawDataSize;
        pCurrRawData += currRawDataSize;

        // Check whether memory available for next read
        if (calcRawDataSize < singleReportSize) {
            break;
        }
        maxReportCount -= static_cast<uint32_t>(currRawDataSize / singleReportSize);
        currRawDataSize = calcRawDataSize;
    }

    *pRawDataSize = pCurrRawData - pRawData;
    return result;
}

ze_result_t MultiDeviceIpSamplingMetricStreamerImp::close() {

    ze_result_t result = ZE_RESULT_SUCCESS;
    for (auto &streamer : subDeviceStreamers) {
        result = streamer->close();
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }
    }

    subDeviceStreamers.clear();
    detachEvent();
    delete this;
    return result;
}

Event::State MultiDeviceIpSamplingMetricStreamerImp::getNotificationState() {

    Event::State state = Event::State::STATE_INITIAL;
    for (auto &streamer : subDeviceStreamers) {
        state = streamer->getNotificationState();
        if (state == Event::State::STATE_SIGNALED) {
            break;
        }
    }

    return state;
}

} // namespace L0
