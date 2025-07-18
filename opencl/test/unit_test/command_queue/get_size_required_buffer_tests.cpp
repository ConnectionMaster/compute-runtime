/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_allocation_properties.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_kernel_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

struct GetSizeRequiredBufferTest : public CommandEnqueueFixture,
                                   public SimpleArgKernelFixture,
                                   public HelloWorldKernelFixture,
                                   public ::testing::Test {

    using HelloWorldKernelFixture::setUp;
    using SimpleArgKernelFixture::setUp;

    void SetUp() override {
        CommandEnqueueFixture::setUp();
        SimpleArgKernelFixture::setUp(pClDevice);
        HelloWorldKernelFixture::setUp(pClDevice, "CopyBuffer_simd", "CopyBuffer");
        BufferDefaults::context = new MockContext;
        srcBuffer = BufferHelper<>::create();
        dstBuffer = BufferHelper<>::create();
        patternAllocation = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});
        pDevice->setPreemptionMode(PreemptionMode::Disabled);
    }

    void TearDown() override {
        context->getMemoryManager()->freeGraphicsMemory(patternAllocation);
        delete dstBuffer;
        delete srcBuffer;
        delete BufferDefaults::context;
        HelloWorldKernelFixture::tearDown();
        SimpleArgKernelFixture::tearDown();
        CommandEnqueueFixture::tearDown();
    }

    Buffer *srcBuffer = nullptr;
    Buffer *dstBuffer = nullptr;
    GraphicsAllocation *patternAllocation = nullptr;
};

HWTEST_F(GetSizeRequiredBufferTest, WhenFillingBufferThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::fillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueFillBufferTraits::offset, 0, 0};
    dc.size = {EnqueueFillBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_FILL_BUFFER, CsrDependencies(), false, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % FamilyType::indirectDataAlignment);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, WhenCopyingBufferThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueCopyBufferHelper<>::enqueue(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_COPY_BUFFER, CsrDependencies(), false, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % FamilyType::indirectDataAlignment);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, WhenReadingBufferNonBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    EnqueueReadBufferHelper<>::enqueueReadBuffer(
        pCmdQ,
        srcBuffer,
        CL_FALSE);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.dstPtr = EnqueueReadBufferTraits::hostPtr;
    dc.srcMemObj = srcBuffer;
    dc.srcOffset = {EnqueueReadBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_READ_BUFFER, CsrDependencies(), false, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % FamilyType::indirectDataAlignment);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, WhenReadingBufferBlockingThenThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueReadBufferHelper<>::enqueueReadBuffer(
        pCmdQ,
        srcBuffer,
        CL_TRUE);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.dstPtr = EnqueueReadBufferTraits::hostPtr;
    dc.srcMemObj = srcBuffer;
    dc.srcOffset = {EnqueueReadBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_READ_BUFFER, CsrDependencies(), false, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(0u, expectedSizeIOH % FamilyType::indirectDataAlignment);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, WhenWritingBufferNonBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
        pCmdQ,
        dstBuffer,
        CL_FALSE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {dstBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_WRITE_BUFFER, CsrDependencies(), false, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, WhenWritingBufferBlockingThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    USE_REAL_FILE_SYSTEM();
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    dstBuffer->forceDisallowCPUCopy = true;
    auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
        pCmdQ,
        dstBuffer,
        CL_TRUE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {dstBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_WRITE_BUFFER, CsrDependencies(), false, false,
                                                                               false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(expectedSizeIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(expectedSizeSSH, usedAfterSSH - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, GivenOutEventForSingleDeviceContextWhenCalculatingCSSizeThenExtraPipeControlIsNotAdded) {
    UltClDeviceFactory deviceFactory{1, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);

    cl_device_id devices[] = {deviceFactory.rootDevices[0]};

    MockContext pContext(ClDeviceVector(devices, 1));
    MockKernelWithInternals mockKernel(*pContext.getDevices()[0]);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);
    auto event = std::make_unique<MockEvent<Event>>(&pContext, nullptr, 0, 0, 0);
    cl_event clEvent = event.get();
    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_MARKER, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_MARKER, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, &clEvent);

    EXPECT_EQ(baseCommandStreamSize, extendedCommandStreamSize);
}

HWTEST_F(GetSizeRequiredBufferTest, GivenUserEventForMultiDeviceContextWhenCalculatingCSSizeThenExtraPipeControlIsNotAdded) {
    UltClDeviceFactory deviceFactory{2, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);

    cl_device_id devices[] = {deviceFactory.rootDevices[0],
                              deviceFactory.rootDevices[1]};

    MockContext pContext(ClDeviceVector(devices, 2));
    MockKernelWithInternals mockKernel(*pContext.getDevices()[0]);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);
    auto userEvent1 = std::make_unique<UserEvent>(&pContext);
    cl_event clEvent = userEvent1.get();
    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_MARKER, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_MARKER, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, &clEvent);

    EXPECT_EQ(baseCommandStreamSize, extendedCommandStreamSize);
}

HWTEST_F(GetSizeRequiredBufferTest, GivenOutEventForMultiDeviceContextWhenCalculatingCSSizeThenExtraPipeControlIsAdded) {
    UltClDeviceFactory deviceFactory{2, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);

    cl_device_id devices[] = {deviceFactory.rootDevices[0],
                              deviceFactory.rootDevices[1]};

    MockContext pContext(ClDeviceVector(devices, 2));
    MockKernelWithInternals mockKernel(*pContext.getDevices()[0]);
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo(mockKernel.mockKernel);
    dispatchInfo.setKernel(mockKernel.mockKernel);
    multiDispatchInfo.push(dispatchInfo);
    auto event = std::make_unique<MockEvent<Event>>(&pContext, nullptr, 0, 0, 0);
    cl_event clEvent = event.get();
    auto baseCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_MARKER, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, nullptr);
    auto extendedCommandStreamSize = EnqueueOperation<FamilyType>::getTotalSizeRequiredCS(CL_COMMAND_MARKER, {}, false, false, false, *pCmdQ, multiDispatchInfo, false, false, false, &clEvent);

    EXPECT_EQ(baseCommandStreamSize + MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pContext.getDevices()[0]->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData), extendedCommandStreamSize);
}

HWTEST2_F(GetSizeRequiredBufferTest, givenMultipleKernelRequiringSshWhenTotalSizeIsComputedThenItIsProperlyAligned, IsHeapfulSupported) {
    USE_REAL_FILE_SYSTEM();
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = dstBuffer;
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {dstBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);

    builder.buildDispatchInfos(multiDispatchInfo);
    builder.buildDispatchInfos(multiDispatchInfo);
    builder.buildDispatchInfos(multiDispatchInfo);
    builder.buildDispatchInfos(multiDispatchInfo);

    auto sizeSSH = multiDispatchInfo.begin()->getKernel()->getSurfaceStateHeapSize();
    sizeSSH += sizeSSH ? FamilyType::BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE : 0;

    sizeSSH = alignUp(sizeSSH, MemoryConstants::cacheLineSize);

    sizeSSH *= 4u;
    sizeSSH = alignUp(sizeSSH, MemoryConstants::pageSize);

    EXPECT_EQ(4u, multiDispatchInfo.size());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);
    EXPECT_EQ(sizeSSH, expectedSizeSSH);
}

HWTEST_F(GetSizeRequiredBufferTest, GivenHelloWorldKernelWhenEnqueingKernelThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    typedef HelloWorldKernelFixture KernelFixture;
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    size_t workSize[] = {64};
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        KernelFixture::pKernel,
        1,
        nullptr,
        workSize,
        workSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto usedAfterCS = commandStream.getUsed();
    auto dshAfter = pDSH->getUsed();
    auto iohAfter = pIOH->getUsed();
    auto sshAfter = pSSH->getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, KernelFixture::pKernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*KernelFixture::pKernel);
    size_t localWorkSizes[] = {64, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*KernelFixture::pKernel, localWorkSizes, pClDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*KernelFixture::pKernel);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, dshAfter - dshBefore);
    EXPECT_GE(expectedSizeIOH, iohAfter - iohBefore);
    EXPECT_GE(expectedSizeSSH, sshAfter - sshBefore);
}

HWTEST_F(GetSizeRequiredBufferTest, GivenKernelWithSimpleArgWhenEnqueingKernelThenHeapsAndCommandBufferConsumedMinimumRequiredSize) {
    typedef SimpleArgKernelFixture KernelFixture;
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    size_t workSize[] = {64};
    auto retVal = EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        KernelFixture::pKernel,
        1,
        nullptr,
        workSize,
        workSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto usedAfterCS = commandStream.getUsed();
    auto dshAfter = pDSH->getUsed();
    auto iohAfter = pIOH->getUsed();
    auto sshAfter = pSSH->getUsed();

    auto expectedSizeCS = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, KernelFixture::pKernel, {});
    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*KernelFixture::pKernel);
    size_t localWorkSizes[] = {64, 1, 1};
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*KernelFixture::pKernel, localWorkSizes, pClDevice->getRootDeviceEnvironment());
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*KernelFixture::pKernel);

    EXPECT_EQ(0u, expectedSizeIOH % FamilyType::indirectDataAlignment);
    EXPECT_EQ(0u, expectedSizeDSH % 64);

    // Since each enqueue* may flush, we may see a MI_BATCH_BUFFER_END appended.
    expectedSizeCS += sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSizeCS = alignUp(expectedSizeCS, MemoryConstants::cacheLineSize);

    EXPECT_GE(expectedSizeCS, usedAfterCS - usedBeforeCS);
    EXPECT_GE(expectedSizeDSH, dshAfter - dshBefore);
    EXPECT_GE(expectedSizeIOH, iohAfter - iohBefore);
    EXPECT_GE(expectedSizeSSH, sshAfter - sshBefore);
}