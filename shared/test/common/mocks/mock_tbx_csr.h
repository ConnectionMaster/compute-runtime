/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/page_fault_manager/tbx_page_fault_manager.h"

namespace NEO {

template <typename GfxFamily>
class MockTbxCsr : public TbxCommandStreamReceiverHw<GfxFamily> {
  public:
    using TbxCommandStreamReceiverHw<GfxFamily>::writeMemory;
    using TbxCommandStreamReceiverHw<GfxFamily>::allocationsForDownload;
    using TbxCommandStreamReceiverHw<GfxFamily>::getParametersForMemory;
    using TbxCommandStreamReceiverHw<GfxFamily>::getTbxPageFaultManager;
    MockTbxCsr(ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : TbxCommandStreamReceiverHw<GfxFamily>(executionEnvironment, 0, deviceBitfield) {
        this->downloadAllocationImpl = [this](GraphicsAllocation &gfxAllocation) {
            this->downloadAllocationTbxMock(gfxAllocation);
        };
    }
    ~MockTbxCsr() override {
        this->downloadAllocationImpl = nullptr;
    }

    void initializeEngine() override {
        TbxCommandStreamReceiverHw<GfxFamily>::initializeEngine();
        initializeEngineCalled = true;
    }

    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        CommandStreamReceiverSimulatedHw<GfxFamily>::writeMemoryWithAubManager(graphicsAllocation, isChunkCopy, gpuVaChunkOffset, chunkSize);
        writeMemoryWithAubManagerCalled = true;
    }

    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override {
        TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(gpuAddress, cpuAddress, size, memoryBank, entryBits);
        writeMemoryCalled = true;
    }
    bool writeMemory(GraphicsAllocation &graphicsAllocation) override {
        writeMemoryGfxAllocCalled = true;
        return TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(graphicsAllocation);
    }

    void submitBatchBufferTbx(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead) override {
        TbxCommandStreamReceiverHw<GfxFamily>::submitBatchBufferTbx(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, entryBits, overrideRingHead);
        overrideRingHeadPassed = overrideRingHead;
        submitBatchBufferCalled = true;
    }
    void pollForCompletion(bool skipTaskCountCheck) override {
        TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion(skipTaskCountCheck);
        pollForCompletionCalled = true;
    }
    void downloadAllocationTbxMock(GraphicsAllocation &gfxAllocation) {
        TbxCommandStreamReceiverHw<GfxFamily>::downloadAllocationTbx(gfxAllocation);
        makeCoherentCalled = true;
    }
    void dumpAllocation(GraphicsAllocation &gfxAllocation) override {
        TbxCommandStreamReceiverHw<GfxFamily>::dumpAllocation(gfxAllocation);
        dumpAllocationCalled = true;
    }

    bool initializeEngineCalled = false;
    bool writeMemoryWithAubManagerCalled = false;
    bool writeMemoryCalled = false;
    bool writeMemoryGfxAllocCalled = false;
    bool submitBatchBufferCalled = false;
    bool overrideRingHeadPassed = false;
    bool pollForCompletionCalled = false;
    bool expectMemoryEqualCalled = false;
    bool expectMemoryNotEqualCalled = false;
    bool makeCoherentCalled = false;
    bool dumpAllocationCalled = false;
};

template <typename GfxFamily>
struct MockTbxCsrRegisterDownloadedAllocations : TbxCommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiver::downloadAllocationImpl;
    using CommandStreamReceiver::downloadAllocations;
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::tagsMultiAllocation;
    using TbxCommandStreamReceiverHw<GfxFamily>::flushSubmissionsAndDownloadAllocations;

    MockTbxCsrRegisterDownloadedAllocations(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : TbxCommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield) {
        this->downloadAllocationImpl = [this](GraphicsAllocation &gfxAllocation) {
            this->downloadAllocationTbxMock(gfxAllocation);
        };
    }
    ~MockTbxCsrRegisterDownloadedAllocations() override {
        this->downloadAllocationImpl = nullptr;
    }
    void downloadAllocationTbxMock(GraphicsAllocation &gfxAllocation) {
        *reinterpret_cast<TaskCountType *>(CommandStreamReceiver::getTagAllocation()->getUnderlyingBuffer()) = this->latestFlushedTaskCount;
        downloadedAllocations.insert(&gfxAllocation);
    }
    bool flushBatchedSubmissions() override {
        flushBatchedSubmissionsCalled = true;
        return true;
    }
    SubmissionStatus flushTagUpdate() override {
        flushTagCalled = true;
        return SubmissionStatus::success;
    }

    std::unique_lock<CommandStreamReceiver::MutexType> obtainUniqueOwnership() override {
        obtainUniqueOwnershipCalled++;
        return TbxCommandStreamReceiverHw<GfxFamily>::obtainUniqueOwnership();
    }

    uint64_t getNonBlockingDownloadTimeoutMs() const override {
        return 1;
    }

    std::set<GraphicsAllocation *> downloadedAllocations;
    bool flushBatchedSubmissionsCalled = false;
    bool flushTagCalled = false;
    size_t obtainUniqueOwnershipCalled = 0;
};
} // namespace NEO
