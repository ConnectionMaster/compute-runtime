/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"

#include <array>

namespace NEO {
namespace DrmEngineMappingHelper {
constexpr std::array<aub_stream::EngineType, 9> engineMapping = {{aub_stream::ENGINE_BCS, aub_stream::ENGINE_BCS1, aub_stream::ENGINE_BCS2,
                                                                  aub_stream::ENGINE_BCS3, aub_stream::ENGINE_BCS4, aub_stream::ENGINE_BCS5,
                                                                  aub_stream::ENGINE_BCS6, aub_stream::ENGINE_BCS7, aub_stream::ENGINE_BCS8}};
} // namespace DrmEngineMappingHelper

EngineInfo::EngineInfo(Drm *drm, HardwareInfo *hwInfo, const std::vector<EngineCapabilities> &engineInfos)
    : engines(engineInfos), tileToEngineToInstanceMap(1) {
    auto computeEngines = 0u;
    BcsInfoMask bcsInfoMask = 0;
    uint32_t numHostLinkCopyEngines = 0;
    uint32_t numScaleUpLinkCopyEngines = 0;

    for (const auto &engineInfo : engineInfos) {
        auto &engine = engineInfo.engine;
        tileToEngineMap.emplace(0, engine);
        switch (engine.engineClass) {
        case I915_ENGINE_CLASS_RENDER:
            tileToEngineToInstanceMap[0][EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo)] = engine;
            break;
        case I915_ENGINE_CLASS_COPY:
            assignCopyEngine(EngineInfo::getBaseCopyEngineType(engineInfo.capabilities), 0, engine,
                             bcsInfoMask, numHostLinkCopyEngines, numScaleUpLinkCopyEngines);
            break;
        default:
            if (engine.engineClass == IoctlHelper::get(drm)->getComputeEngineClass()) {
                tileToEngineToInstanceMap[0][static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + computeEngines)] = engine;
                computeEngines++;
            }
            break;
        }
    }
    setSupportedEngiesInfo(hwInfo, computeEngines, bcsInfoMask);
}

EngineInfo::EngineInfo(Drm *drm, HardwareInfo *hwInfo, uint32_t tileCount, const std::vector<DistanceInfo> &distanceInfos, const std::vector<drm_i915_query_item> &queryItems, const std::vector<EngineCapabilities> &engineInfos)
    : engines(engineInfos), tileToEngineToInstanceMap(tileCount) {
    auto tile = 0u;
    auto computeEnginesPerTile = 0u;
    auto copyEnginesPerTile = 0u;
    for (auto i = 0u; i < distanceInfos.size(); i++) {
        if (i > 0u && distanceInfos[i].region.memoryInstance != distanceInfos[i - 1u].region.memoryInstance) {
            tile++;
            computeEnginesPerTile = 0u;
            copyEnginesPerTile = 0u;
        }
        if (queryItems[i].length < 0 || distanceInfos[i].distance != 0)
            continue;

        auto engine = distanceInfos[i].engine;
        tileToEngineMap.emplace(tile, engine);
        switch (engine.engineClass) {
        case I915_ENGINE_CLASS_RENDER:
            tileToEngineToInstanceMap[tile][EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo)] = engine;
            break;
        case I915_ENGINE_CLASS_COPY:
            tileToEngineToInstanceMap[tile][DrmEngineMappingHelper::engineMapping[copyEnginesPerTile]] = engine;
            copyEnginesPerTile++;
            break;
        default:
            if (engine.engineClass == IoctlHelper::get(drm)->getComputeEngineClass()) {
                tileToEngineToInstanceMap[tile][static_cast<aub_stream::EngineType>(aub_stream::ENGINE_CCS + computeEnginesPerTile)] = engine;
                computeEnginesPerTile++;
            }
            break;
        }
    }

    BcsInfoMask bcsInfoMask = maxNBitValue(copyEnginesPerTile);
    setSupportedEngiesInfo(hwInfo, computeEnginesPerTile, bcsInfoMask);
}

const EngineClassInstance *EngineInfo::getEngineInstance(uint32_t tile, aub_stream::EngineType engineType) const {
    if (tile >= tileToEngineToInstanceMap.size()) {
        return nullptr;
    }
    auto &engineToInstanceMap = tileToEngineToInstanceMap[tile];
    auto iter = engineToInstanceMap.find(engineType);
    if (iter == engineToInstanceMap.end()) {
        return nullptr;
    }
    return &iter->second;
}

void EngineInfo::setSupportedEngiesInfo(HardwareInfo *hwInfo, uint32_t numComputeEngines, const BcsInfoMask &bcsInfoMask) {
    auto &ccsInfo = hwInfo->gtSystemInfo.CCSInfo;

    if (numComputeEngines > 0u) {
        hwInfo->featureTable.flags.ftrCCSNode = true;

        ccsInfo.IsValid = true;
        ccsInfo.NumberOfCCSEnabled = numComputeEngines;
        ccsInfo.Instances.CCSEnableMask = static_cast<uint32_t>(maxNBitValue(numComputeEngines));
    } else {
        hwInfo->capabilityTable.defaultEngineType = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *hwInfo);
        hwInfo->featureTable.flags.ftrCCSNode = false;

        ccsInfo.IsValid = false;
        ccsInfo.NumberOfCCSEnabled = 0;
        ccsInfo.Instances.CCSEnableMask = 0;
    }
    hwInfo->featureTable.ftrBcsInfo = bcsInfoMask;
}

uint32_t EngineInfo::getEngineTileIndex(const EngineClassInstance &engine) {
    uint32_t tile = 0;
    if (tileToEngineMap.empty()) {
        return tile; //Empty map
    }

    for (auto itr = tileToEngineMap.begin(); itr != tileToEngineMap.end(); itr++) {
        if ((itr->second.engineClass == engine.engineClass) && (itr->second.engineInstance == engine.engineInstance)) {
            tile = itr->first;
            break;
        }
    }
    return tile;
}

void EngineInfo::getListOfEnginesOnATile(uint32_t tile, std::vector<EngineClassInstance> &listOfEngines) {
    auto range = tileToEngineMap.equal_range(tile);
    for (auto itr = range.first; itr != range.second; ++itr) {
        listOfEngines.push_back(itr->second);
    }
}

} // namespace NEO
