/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_lnl.h"

constexpr static auto gfxProduct = IGFX_LUNARLAKE;

#include "shared/source/xe2_hpg_core/lnl/os_agnostic_product_helper_lnl.inl"
#include "shared/source/xe2_hpg_core/os_agnostic_product_helper_xe2_hpg_core.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::restartDirectSubmissionForHostptrFree() const {
    return true;
}

template <>
void ProductHelperHw<gfxProduct>::overrideDirectSubmissionTimeouts(std::chrono::microseconds &timeout, std::chrono::microseconds &maxTimeout) const {
    timeout = std::chrono::microseconds{1'000};
    maxTimeout = std::chrono::microseconds{1'000};
    if (debugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeout = std::chrono::microseconds{debugManager.flags.DirectSubmissionControllerTimeout.get()};
    }
    if (debugManager.flags.DirectSubmissionControllerMaxTimeout.get() != -1) {
        maxTimeout = std::chrono::microseconds{debugManager.flags.DirectSubmissionControllerMaxTimeout.get()};
    }
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
