/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe_prelim.h"

namespace NEO {

bool IoctlHelperXePrelim::isMediaGt(uint16_t gtType) const {
    return IoctlHelperXe::isMediaGt(gtType);
}

bool IoctlHelperXePrelim::isPrimaryContext(const OsContextLinux &osContext, uint32_t deviceIndex) {
    return false;
}

uint32_t IoctlHelperXePrelim::getPrimaryContextId(const OsContextLinux &osContext, uint32_t deviceIndex, size_t contextIndex) {
    return static_cast<uint32_t>(-1);
}

void IoctlHelperXePrelim::setContextProperties(const OsContextLinux &osContext, uint32_t deviceIndex, void *extProperties, uint32_t &extIndexInOut) {
    IoctlHelperXe::setContextProperties(osContext, deviceIndex, extProperties, extIndexInOut);
}

bool IoctlHelperXePrelim::getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) {
    return IoctlHelperXe::getFdFromVmExport(vmId, flags, fd);
}

unsigned int IoctlHelperXePrelim::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    return IoctlHelperXe::getIoctlRequestValue(ioctlRequest);
}

std::string IoctlHelperXePrelim::getIoctlString(DrmIoctl ioctlRequest) const {
    return IoctlHelperXe::getIoctlString(ioctlRequest);
}

} // namespace NEO
