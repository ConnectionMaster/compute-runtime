/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bcs_ccs_dependency_pair_container.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/test/unit_test/fixtures/dispatch_flags_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include <array>
#include <memory>

namespace NEO {
class Device;
class ExecutionEnvironment;
class Kernel;
} // namespace NEO

using namespace NEO;

TEST(CommandTest, GivenNoTerminateFlagWhenSubmittingMapUnmapThenCsrIsFlushed) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr, false));

    MockBuffer buffer;
    auto initialTaskCount = 0u;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::map, buffer, size, offset, false, *cmdQ));
    CompletionStamp completionStamp = command->submit(20, false);

    auto expectedTaskCount = initialTaskCount + 1;
    if (cmdQ->heaplessStateInitEnabled) {
        expectedTaskCount++;
    }
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, GivenTerminateFlagWhenSubmittingMapUnmapThenFlushIsAborted) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr, false));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::map, buffer, size, offset, false, *cmdQ));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, GivenNoTerminateFlagWhenSubmittingMarkerThenCsrIsNotFlushed) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr, false));
    MockBuffer buffer;

    auto heaplessStateInit = cmdQ->getHeaplessStateInitEnabled();
    auto initialTaskCount = heaplessStateInit ? 1u : 0u;

    std::unique_ptr<Command> command(new CommandWithoutKernel(*cmdQ));
    CompletionStamp completionStamp = command->submit(20, false);

    EXPECT_EQ(initialTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, GivenTerminateFlagWhenSubmittingMarkerThenFlushIsAborted) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr, false));
    MockCommandStreamReceiver csr(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    MockBuffer buffer;

    auto initialTaskCount = csr.peekTaskCount();
    std::unique_ptr<Command> command(new CommandWithoutKernel(*cmdQ));
    CompletionStamp completionStamp = command->submit(20, true);

    auto submitTaskCount = csr.peekTaskCount();
    EXPECT_EQ(initialTaskCount, submitTaskCount);

    auto expectedTaskCount = 0u;
    EXPECT_EQ(expectedTaskCount, completionStamp.taskCount);
}

TEST(CommandTest, GivenGpuHangWhenSubmittingMapUnmapCommandsThenReturnedCompletionStampIndicatesGpuHang) {
    for (const auto operationType : {MapOperationType::map, MapOperationType::unmap}) {
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

        std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr, false));
        cmdQ->waitUntilCompleteReturnValue = WaitStatus::gpuHang;

        MemObjSizeArray size = {{1, 1, 1}};
        MemObjOffsetArray offset = {{0, 0, 0}};

        MockBuffer buffer;
        buffer.isZeroCopy = false;
        buffer.callBaseTransferDataToHostPtr = false;
        buffer.callBaseTransferDataFromHostPtr = false;

        std::unique_ptr<Command> command(new CommandMapUnmap(operationType, buffer, size, offset, false, *cmdQ));
        CompletionStamp completionStamp = command->submit(20, false);

        EXPECT_EQ(1, cmdQ->waitUntilCompleteCalledCount);
        EXPECT_EQ(CompletionStamp::gpuHang, completionStamp.taskCount);

        EXPECT_EQ(0, buffer.transferDataToHostPtrCalledCount);
        EXPECT_EQ(0, buffer.transferDataFromHostPtrCalledCount);
    }
}

TEST(CommandTest, GivenNoGpuHangWhenSubmittingMapUnmapCommandsThenReturnedCompletionStampDoesNotIndicateGpuHang) {
    constexpr size_t operationTypesCount{2};
    constexpr static std::array<MapOperationType, operationTypesCount> operationTypes{MapOperationType::map, MapOperationType::unmap};
    constexpr static std::array<std::pair<int, int>, operationTypesCount> expectedCallsCounts = {
        std::pair{1, 0}, std::pair{0, 1}};

    for (auto i = 0u; i < operationTypesCount; ++i) {
        const auto operationType = operationTypes[i];
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

        std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(nullptr, device.get(), nullptr, false));
        cmdQ->waitUntilCompleteReturnValue = WaitStatus::ready;

        MemObjSizeArray size = {{1, 1, 1}};
        MemObjOffsetArray offset = {{0, 0, 0}};

        MockBuffer buffer;
        buffer.isZeroCopy = false;
        buffer.callBaseTransferDataToHostPtr = false;
        buffer.callBaseTransferDataFromHostPtr = false;

        std::unique_ptr<Command> command(new CommandMapUnmap(operationType, buffer, size, offset, false, *cmdQ));
        CompletionStamp completionStamp = command->submit(20, false);

        EXPECT_EQ(1, cmdQ->waitUntilCompleteCalledCount);
        EXPECT_NE(CompletionStamp::gpuHang, completionStamp.taskCount);

        const auto &[expectedTransferDataToHostPtrCalledCount, expectedTransferDataFromHostPtrCalledCount] = expectedCallsCounts[i];
        EXPECT_EQ(expectedTransferDataToHostPtrCalledCount, buffer.transferDataToHostPtrCalledCount);
        EXPECT_EQ(expectedTransferDataFromHostPtrCalledCount, buffer.transferDataFromHostPtrCalledCount);
    }
}

TEST(CommandTest, givenWaitlistRequestWhenCommandComputeKernelIsCreatedThenMakeLocalCopyOfWaitlist) {
    class MockCommandComputeKernel : public CommandComputeKernel {
      public:
        using CommandComputeKernel::eventsWaitlist;
        MockCommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> &surfaces, Kernel *kernel)
            : CommandComputeKernel(commandQueue, kernelOperation, surfaces, false, false, false, nullptr, PreemptionMode::Disabled, kernel, 0, nullptr) {}
    };

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockCommandQueue cmdQ(nullptr, device.get(), nullptr, false);
    MockKernelWithInternals kernel(*device);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);

    UserEvent event1, event2, event3;
    cl_event waitlist[] = {&event1, &event2};
    EventsRequest eventsRequest(2, waitlist, nullptr);

    MockCommandComputeKernel command(cmdQ, kernelOperation, surfaces, kernel);

    event1.incRefInternal();
    event2.incRefInternal();

    command.setEventsRequest(eventsRequest);

    waitlist[1] = &event3;

    EXPECT_EQ(static_cast<cl_event>(&event1), command.eventsWaitlist[0]);
    EXPECT_EQ(static_cast<cl_event>(&event2), command.eventsWaitlist[1]);
}

TEST(KernelOperationDestruction, givenKernelOperationWhenItIsDestructedThenAllAllocationsAreStoredInInternalStorageForReuse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfReusableAllocationsPerCmdQueue.set(0);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockCommandQueue cmdQ(nullptr, device.get(), nullptr, false);
    InternalAllocationStorage &allocationStorage = *device->getDefaultEngine().commandStreamReceiver->getInternalAllocationStorage();
    auto &allocationsForReuse = allocationStorage.getAllocationsForReuse();

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    cmdQ.allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    cmdQ.allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));

    auto &heapAllocation1 = *ih1->getGraphicsAllocation();
    auto &heapAllocation2 = *ih2->getGraphicsAllocation();
    auto &heapAllocation3 = *ih3->getGraphicsAllocation();
    auto &cmdStreamAllocation = *cmdStream->getGraphicsAllocation();

    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, allocationStorage);
    kernelOperation->setHeaps(ih1, ih2, ih3);
    EXPECT_TRUE(allocationsForReuse.peekIsEmpty());

    kernelOperation.reset();
    EXPECT_TRUE(allocationsForReuse.peekContains(cmdStreamAllocation));
    EXPECT_TRUE(allocationsForReuse.peekContains(heapAllocation1));
    EXPECT_TRUE(allocationsForReuse.peekContains(heapAllocation2));
    EXPECT_TRUE(allocationsForReuse.peekContains(heapAllocation3));
}

template <typename GfxFamily>
class MockCsr1 : public CommandStreamReceiverHw<GfxFamily> {
  public:
    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh,
                              const IndirectHeap *ssh, TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        if (this->getHeaplessStateInitEnabled()) {
            return flushTaskHeapless(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
        } else {
            return flushTaskHeapful(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
        }
    }

    CompletionStamp flushTaskHeapless(LinearStream &commandStream, size_t commandStreamStart,
                                      const IndirectHeap *dsh, const IndirectHeap *ioh,
                                      const IndirectHeap *ssh, TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        passedDispatchFlags = dispatchFlags;
        return CompletionStamp();
    }

    CompletionStamp flushTaskHeapful(LinearStream &commandStream, size_t commandStreamStart,
                                     const IndirectHeap *dsh, const IndirectHeap *ioh,
                                     const IndirectHeap *ssh, TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        passedDispatchFlags = dispatchFlags;
        return CompletionStamp();
    }

    MockCsr1(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    DispatchFlags passedDispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    using CommandStreamReceiver::timestampPacketWriteEnabled;
};

HWTEST_F(DispatchFlagsTests, givenCommandMapUnmapWhenSubmitThenPassCorrectDispatchFlags) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    MockBuffer buffer;

    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    std::unique_ptr<Command> command(new CommandMapUnmap(MapOperationType::map, buffer, size, offset, false, *mockCmdQ));
    command->submit(20, false);
    PreemptionFlags flags = {};
    PreemptionMode devicePreemption = mockCmdQ->getDevice().getPreemptionMode();

    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(PreemptionHelper::taskPreemptionMode(devicePreemption, flags), mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(GrfConfig::notApplicable, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::notApplicable, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_EQ(GrfConfig::notApplicable, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.useSLM);
    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_FALSE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    } else {
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    }
    EXPECT_FALSE(mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::low, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenCommandComputeKernelWhenSubmitThenPassCorrectDispatchFlags) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);

    PreemptionMode preemptionMode = device->getPreemptionMode();
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals kernel(*device);
    kernelOperation->setHeaps(ih1, ih2, ih3);

    bool flushDC = false;
    bool slmUsed = false;
    bool ndRangeKernel = false;
    std::unique_ptr<Command> command(new CommandComputeKernel(*mockCmdQ, kernelOperation, surfaces, flushDC, slmUsed, ndRangeKernel, nullptr, preemptionMode, kernel, 1, nullptr));
    command->submit(20, false);

    EXPECT_FALSE(mockCsr->passedDispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode);
    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(preemptionMode, mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(kernel.mockKernel->getKernelInfo().kernelDescriptor.kernelAttributes.numGrfRequired, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::l3CacheOn, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_EQ(flushDC, mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_EQ(slmUsed, mockCsr->passedDispatchFlags.useSLM);
    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_FALSE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    } else {
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    }
    EXPECT_EQ(ndRangeKernel, mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::low, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenClCommandCopyImageWhenSubmitThenFlushTextureCacheHasProperValue) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);

    PreemptionMode preemptionMode = device->getPreemptionMode();
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals kernel(*device);
    kernelOperation->setHeaps(ih1, ih2, ih3);

    bool flushDC = false;
    bool slmUsed = false;
    uint32_t commandType = CL_COMMAND_COPY_IMAGE;
    std::unique_ptr<Command> command(new CommandComputeKernel(*mockCmdQ, kernelOperation, surfaces, flushDC, slmUsed, commandType, nullptr, preemptionMode, kernel, 1, nullptr));
    command->submit(20, false);

    EXPECT_FALSE(mockCsr->passedDispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode);
    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(preemptionMode, mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(kernel.mockKernel->getKernelInfo().kernelDescriptor.kernelAttributes.numGrfRequired, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::l3CacheOn, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_EQ(flushDC, mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_EQ(mockCmdQ->isTextureCacheFlushNeeded(commandType), mockCsr->passedDispatchFlags.textureCacheFlush);
    EXPECT_EQ(slmUsed, mockCsr->passedDispatchFlags.useSLM);

    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_FALSE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    } else {
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    }
    EXPECT_FALSE(mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::low, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenCommandWithoutKernelWhenSubmitThenPassCorrectDispatchFlags) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    mockCsr->timestampPacketWriteEnabled = true;
    mockCmdQ->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    TimestampPacketDependencies timestampPacketDependencies;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);

    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);
    std::unique_ptr<Command> command(new CommandWithoutKernel(*mockCmdQ, kernelOperation, nullptr));
    command->setTimestampPacketNode(*mockCmdQ->timestampPacketContainer, std::move(timestampPacketDependencies));

    command->submit(20, false);

    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
    EXPECT_EQ(mockCmdQ->getThrottle(), mockCsr->passedDispatchFlags.throttle);
    EXPECT_EQ(mockCmdQ->getDevice().getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(GrfConfig::notApplicable, mockCsr->passedDispatchFlags.numGrfRequired);
    EXPECT_EQ(L3CachingSettings::notApplicable, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.blocking);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.dcFlush);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.useSLM);
    if (mockCsr->isUpdateTagFromWaitEnabled()) {
        EXPECT_FALSE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    } else {
        EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    }
    EXPECT_FALSE(mockCsr->passedDispatchFlags.gsba32BitRequired);
    EXPECT_EQ(mockCmdQ->getPriority() == QueuePriority::low, mockCsr->passedDispatchFlags.lowPriority);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_EQ(mockCmdQ->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.epilogueRequired);
}

HWTEST_F(DispatchFlagsTests, givenCsrDependencyWhenSubmitCommandWithoutKernelThenDependencyUpdateWasCalled) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());
    auto dependentCsr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());

    mockCsr->timestampPacketWriteEnabled = true;
    mockCmdQ->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    TimestampPacketDependencies timestampPacketDependencies;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);

    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);
    CsrDependencyContainer dependencyMap;
    auto tag = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag();
    dependencyMap.push_back(std::make_pair(dependentCsr.get(), tag));
    std::unique_ptr<Command> command(new CommandWithoutKernel(*mockCmdQ, kernelOperation, &dependencyMap));
    command->setTimestampPacketNode(*mockCmdQ->timestampPacketContainer, std::move(timestampPacketDependencies));

    command->submit(20, false);
    EXPECT_EQ(dependentCsr->submitDependencyUpdateCalledTimes, 1u);
}

HWTEST_F(DispatchFlagsTests, givenCsrDependencyWhendependencyUpdateReturnsFalseThenSubmitReturnGpuHang) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());
    auto dependentCsr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());

    mockCsr->timestampPacketWriteEnabled = true;
    mockCmdQ->timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    TimestampPacketDependencies timestampPacketDependencies;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);

    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    kernelOperation->setHeaps(ih1, ih2, ih3);
    CsrDependencyContainer dependencyMap;
    auto tag = mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag();
    dependencyMap.push_back(std::make_pair(dependentCsr.get(), tag));
    std::unique_ptr<Command> command(new CommandWithoutKernel(*mockCmdQ, kernelOperation, &dependencyMap));
    command->setTimestampPacketNode(*mockCmdQ->timestampPacketContainer, std::move(timestampPacketDependencies));
    dependentCsr->submitDependencyUpdateReturnValue = false;
    auto stamp = command->submit(20, false);
    EXPECT_EQ(stamp.taskCount, CompletionStamp::gpuHang);
}

HWTEST_F(DispatchFlagsTests, givenCommandComputeKernelWhenSubmitThenPassCorrectDispatchHints) {
    using CsrType = MockCsr1<FamilyType>;
    setUpImpl<CsrType>();
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 1, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 1, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 1, ih3);
    mockCmdQ->dispatchHints = 1234;

    PreemptionMode preemptionMode = device->getPreemptionMode();
    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 1, AllocationType::commandBuffer, device->getDeviceBitfield()}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals kernel(*device);
    kernelOperation->setHeaps(ih1, ih2, ih3);

    bool flushDC = false;
    bool slmUsed = false;
    bool ndRangeKernel = false;
    std::unique_ptr<Command> command(new CommandComputeKernel(*mockCmdQ, kernelOperation, surfaces, flushDC, slmUsed, ndRangeKernel, nullptr, preemptionMode, kernel, 1, nullptr));
    command->submit(20, false);

    EXPECT_TRUE(mockCsr->passedDispatchFlags.epilogueRequired);
    EXPECT_EQ(1234u, mockCsr->passedDispatchFlags.engineHints);
    auto expectedThreadArbitrationPolicy = kernel.mockKernel->getDescriptor().kernelAttributes.threadArbitrationPolicy;
    EXPECT_EQ(expectedThreadArbitrationPolicy, mockCsr->passedDispatchFlags.threadArbitrationPolicy);
}
