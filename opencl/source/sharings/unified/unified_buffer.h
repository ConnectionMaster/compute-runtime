/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/unified/unified_sharing.h"

#include "CL/cl.h"

namespace NEO {
class Buffer;
class Context;
enum class UnifiedSharingHandleType;
struct UnifiedSharingMemoryDescription;

class UnifiedBuffer : public UnifiedSharing {
    using UnifiedSharing::UnifiedSharing;

  public:
    static Buffer *createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription description, cl_int *errcodeRet);
};
} // namespace NEO
