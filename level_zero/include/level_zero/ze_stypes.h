/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZE_STYPES_H
#define _ZE_STYPES_H

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <cstdint>
using ze_structure_type_ext_t = uint32_t;

#define ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC static_cast<ze_structure_type_ext_t>(0x00020020)
#define ZE_STRUCTURE_TYPE_INTEL_MEDIA_COMMUNICATION_DESC static_cast<ze_structure_type_ext_t>(0x00020021)
#define ZE_STRUCTURE_TYPE_INTEL_MEDIA_DOORBELL_HANDLE_DESC static_cast<ze_structure_type_ext_t>(0x00020022)
#define ZE_STRUCTURE_TYPE_INTEL_DEVICE_MEDIA_EXP_PROPERTIES static_cast<ze_structure_type_ext_t>(0x00020023)
#define ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES static_cast<ze_structure_type_ext_t>(0x00030007)
#define ZEX_STRUCTURE_DEVICE_MODULE_REGISTER_FILE_EXP static_cast<ze_structure_type_ext_t>(0x00030010)
#define ZEX_STRUCTURE_KERNEL_REGISTER_FILE_SIZE_EXP static_cast<ze_structure_type_ext_t>(0x00030012)
#define ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES static_cast<ze_structure_type_ext_t>(0x00030013)
#define ZEX_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC static_cast<ze_structure_type_ext_t>(0x00030016)
#define ZE_INTEL_STRUCTURE_TYPE_DEVICE_COMMAND_LIST_WAIT_ON_MEMORY_DATA_SIZE_EXP_DESC static_cast<ze_structure_type_ext_t>(0x00030017)
#define ZEX_INTEL_STRUCTURE_TYPE_QUEUE_ALLOCATE_MSIX_HINT_EXP_PROPERTIES static_cast<ze_structure_type_ext_t>(0x00030018)
#define ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES static_cast<ze_structure_type_ext_t>(0x0003001B)
#define ZE_STRUCTURE_INTEL_DEVICE_MEMORY_CXL_EXP_PROPERTIES static_cast<ze_structure_type_ext_t>(0x00030019)
#define ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC static_cast<ze_structure_type_ext_t>(0x0003001C)
#define ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES static_cast<ze_structure_type_ext_t>(0x0003001D)
#define ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES static_cast<ze_structure_type_ext_t>(0x00030027)

// Metric structure types
#define ZET_INTEL_STRUCTURE_TYPE_METRIC_SOURCE_ID_EXP (zet_structure_type_t)0x0001000a                  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
#define ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULATE_DESC_EXP (zet_structure_type_t)0x00010009             // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
#define ZET_INTEL_STRUCTURE_TYPE_METRIC_GROUP_CALCULATE_EXP_PROPERTIES (zet_structure_type_t)0x00010008 // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901

#endif
