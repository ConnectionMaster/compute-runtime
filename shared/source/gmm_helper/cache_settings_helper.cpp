/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getGmmUsageType(AllocationType allocationType, bool forceUncached, const ProductHelper &productHelper, const HardwareInfo *hwInfo) {
    if (debugManager.flags.ForceUncachedGmmUsageType.get()) {
        UNRECOVERABLE_IF(allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(allocationType) - 1)) & debugManager.flags.ForceUncachedGmmUsageType.get()) {
            forceUncached = true;
        }
    }

    if (forceUncached || debugManager.flags.ForceAllResourcesUncached.get()) {
        return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
    } else {
        return getDefaultUsageTypeWithCachingEnabled(allocationType, productHelper, hwInfo);
    }
}

bool CacheSettingsHelper::preferNoCpuAccess(GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsageType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.EnableCpuCacheForResources.get()) {
        return false;
    }
    if (rootDeviceEnvironment.isWddmOnLinux()) {
        return false;
    }
    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    if (productHelper.isCachingOnCpuAvailable()) {
        return false;
    }
    return (gmmResourceUsageType != GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER);
}

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType, const ProductHelper &productHelper, const HardwareInfo *hwInfo) {
    if (productHelper.overrideUsageForDcFlushMitigation(allocationType)) {
        return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
    }

    if (debugManager.flags.ForceGmmSystemMemoryBufferForAllocations.get()) {
        UNRECOVERABLE_IF(allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(allocationType))) & debugManager.flags.ForceGmmSystemMemoryBufferForAllocations.get()) {
            return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
        }
    }

    if (hwInfo->capabilityTable.isIntegratedDevice) {
        if (AllocationType::ringBuffer == allocationType || AllocationType::semaphoreBuffer == allocationType) {
            return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
        }
    }

    switch (allocationType) {
    case AllocationType::image:
        return GMM_RESOURCE_USAGE_OCL_IMAGE;
    case AllocationType::internalHeap:
    case AllocationType::linearStream:
        if (debugManager.flags.DisableCachingForHeaps.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
        }
        return GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
    case AllocationType::constantSurface:
        if (debugManager.flags.ForceL1Caching.get() == 0) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
    case AllocationType::buffer:
    case AllocationType::sharedBuffer:
    case AllocationType::svmGpu:
    case AllocationType::unifiedSharedMemory:
        if (debugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    case AllocationType::bufferHostMemory:
    case AllocationType::externalHostPtr:
    case AllocationType::fillPattern:
    case AllocationType::internalHostMemory:
    case AllocationType::mapAllocation:
    case AllocationType::svmCpu:
    case AllocationType::svmZeroCopy:
    case AllocationType::tagBuffer:
        if (debugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
        }
        return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
    case AllocationType::gpuTimestampDeviceBuffer:
    case AllocationType::timestampPacketTagBuffer:
        if (debugManager.flags.ForceNonCoherentModeForTimestamps.get()) {
            return GMM_RESOURCE_USAGE_OCL_BUFFER;
        }
        if (productHelper.isDcFlushAllowed()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType, productHelper);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    default:
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    }
}

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getDefaultUsageTypeWithCachingDisabled(AllocationType allocationType, const ProductHelper &productHelper) {
    switch (allocationType) {
    case AllocationType::preemption:
        return GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    case AllocationType::internalHeap:
    case AllocationType::linearStream:
        return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
    case AllocationType::timestampPacketTagBuffer:
    case AllocationType::gpuTimestampDeviceBuffer:
        if (debugManager.flags.ForceNonCoherentModeForTimestamps.get()) {
            return GMM_RESOURCE_USAGE_OCL_BUFFER;
        }
        [[fallthrough]];
    default:
        return productHelper.isNewCoherencyModelSupported() ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }
}

// Set 2-way coherency for allocations which are not aligned to cacheline
GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getGmmUsageTypeForUserPtr(bool isCacheFlushRequired, const void *userPtr, size_t size, const ProductHelper &productHelper) {
    if (isCacheFlushRequired && !isL3Capable(userPtr, size) && productHelper.isMisalignedUserPtr2WayCoherent()) {
        return GMM_RESOURCE_USAGE_HW_CONTEXT;
    } else {
        return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
    }
}

} // namespace NEO
