/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

namespace L0 {
struct DebugSessionLinux : DebugSessionImp {

    DebugSessionLinux(const zet_debug_config_t &config, Device *device, int fd) : DebugSessionImp(config, device), fd(fd){};
    static ze_result_t translateDebuggerOpenErrno(int error);
    bool closeFd();
    void closeAsyncThread();

    int fd = 0;
    std::atomic<bool> internalThreadHasStarted{false};
    static void *readInternalEventsThreadFunction(void *arg);

    MOCKABLE_VIRTUAL void startInternalEventsThread() {
        internalEventThread.thread = NEO::Thread::create(readInternalEventsThreadFunction, reinterpret_cast<void *>(this));
    }
    void closeInternalEventsThread() {
        internalEventThread.close();
    }

    virtual void readInternalEventsAsync() = 0;
    MOCKABLE_VIRTUAL std::unique_ptr<uint64_t[]> getInternalEvent();
    MOCKABLE_VIRTUAL float getThreadStartLimitTime() {
        return 0.5;
    }
    virtual int openVmFd(uint64_t vmHandle, bool readOnly) = 0;
    virtual int flushVmCache(int vmfd) { return 0; };
    ze_result_t readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) override;
    ze_result_t writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) override;

    ThreadHelper internalEventThread;
    std::mutex internalEventThreadMutex;
    std::condition_variable internalEventCondition;
    std::queue<std::unique_ptr<uint64_t[]>> internalEventQueue;
    constexpr static uint64_t invalidClientHandle = std::numeric_limits<uint64_t>::max();
    constexpr static uint64_t invalidHandle = std::numeric_limits<uint64_t>::max();
    uint64_t clientHandle = invalidClientHandle;
    uint64_t clientHandleClosed = invalidClientHandle;
    static constexpr size_t maxEventSize = 4096;

    struct IoctlHandler {
        virtual ~IoctlHandler() = default;
        virtual int ioctl(int fd, unsigned long request, void *arg) {
            return 0;
        };
        MOCKABLE_VIRTUAL int fsync(int fd) {
            return NEO::SysCalls::fsync(fd);
        }
        MOCKABLE_VIRTUAL int poll(pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
            return NEO::SysCalls::poll(pollFd, numberOfFds, timeout);
        }

        MOCKABLE_VIRTUAL int64_t pread(int fd, void *buf, size_t count, off_t offset) {
            return NEO::SysCalls::pread(fd, buf, count, offset);
        }

        MOCKABLE_VIRTUAL int64_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
            return NEO::SysCalls::pwrite(fd, buf, count, offset);
        }

        MOCKABLE_VIRTUAL void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
            return NEO::SysCalls::mmap(addr, size, prot, flags, fd, off);
        }

        MOCKABLE_VIRTUAL int munmap(void *addr, size_t size) {
            return NEO::SysCalls::munmap(addr, size);
        }
    };

  protected:
    enum class ThreadControlCmd {
        interrupt,
        resume,
        stopped,
        interruptAll
    };

    virtual int euControlIoctl(ThreadControlCmd threadCmd,
                               const NEO::EngineClassInstance *classInstance,
                               std::unique_ptr<uint8_t[]> &bitmask,
                               size_t bitmaskSize, uint64_t &seqnoOut, uint64_t &bitmaskSizeOut) = 0;
    MOCKABLE_VIRTUAL int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize);
    void checkStoppedThreadsAndGenerateEvents(const std::vector<EuThread::ThreadId> &threads, uint64_t memoryHandle, uint32_t deviceIndex) override;
    MOCKABLE_VIRTUAL bool checkForceExceptionBit(uint64_t memoryHandle, EuThread::ThreadId threadId, uint32_t *cr0, const SIP::regset_desc *regDesc);
    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override;
    ze_result_t interruptImp(uint32_t deviceIndex) override;
    std::unique_ptr<IoctlHandler> ioctlHandler;
    uint64_t euControlInterruptSeqno[NEO::EngineLimits::maxHandleCount];
};
} // namespace L0