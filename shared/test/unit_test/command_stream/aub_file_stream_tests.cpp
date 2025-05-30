/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/neo_driver_version.h"
#include "shared/source/memory_manager/page_table.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_file_stream.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "driver_version.h"
#include "gtest/gtest.h"
#include "sys_calls.h"

#include <fstream>
#include <memory>

using namespace NEO;

using AubFileStreamTests = Test<AubCommandStreamReceiverFixture>;
using AubFileStreamWithoutAubStreamTests = Test<AubCommandStreamReceiverWithoutAubStreamFixture>;

struct AddPatchInfoCommentsAubTests : AubFileStreamWithoutAubStreamTests {
    void SetUp() override {
        debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
        AubFileStreamWithoutAubStreamTests::SetUp();
    }

    DebugManagerStateRestore restore;
};

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitFileIsCalledWithInvalidFileNameThenFileIsNotOpened) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string invalidFileName = "";

    EXPECT_THROW(aubCsr->initFile(invalidFileName), std::exception);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithoutAubManagerWhenInitFileIsCalledWithInvalidFileNameThenFileIsNotOpened) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, executionEnvironment, 0, deviceBitfield);
    std::string invalidFileName = "";
    aubCsr->aubManager = nullptr;

    EXPECT_THROW(aubCsr->initFile(invalidFileName), std::exception);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitFileIsCalledThenFileIsOpenedAndFileNameIsStored) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";

    aubCsr->initFile(fileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->closeFile();
    EXPECT_FALSE(aubCsr->isFileOpen());
    EXPECT_TRUE(aubCsr->getFileName().empty());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenReopenFileIsCalledThenFileWithSpecifiedNameIsReopened) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    std::string newFileName = "new_file_name.aub";

    aubCsr->reopenFile(fileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());

    aubCsr->reopenFile(newFileName);
    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_STREQ(newFileName.c_str(), aubCsr->getFileName().c_str());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithoutAubManagerWhenInitFileIsCalledThenFileShouldBeInitializedWithHeaderOnce) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    aubCsr->aubManager = nullptr;
    aubCsr->stream = mockAubFileStream.get();

    aubCsr->initFile(fileName);
    aubCsr->initFile(fileName);

    EXPECT_EQ(1u, mockAubFileStream->openCalledCnt);
    EXPECT_EQ(1u, mockAubFileStream->initCalledCnt);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenInitFileIsCalledThenFileShouldBeInitializedOnce) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    aubCsr->aubManager = mockAubManager.get();

    aubCsr->initFile(fileName);
    aubCsr->initFile(fileName);

    EXPECT_EQ(1u, mockAubManager->openCalledCnt);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithoutAubManagerWhenFileFunctionsAreCalledThenTheyShouldCallTheExpectedAubManagerFunctions) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    aubCsr->aubManager = nullptr;
    aubCsr->stream = mockAubFileStream.get();

    aubCsr->initFile(fileName);
    EXPECT_EQ(1u, mockAubFileStream->initCalledCnt);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_TRUE(mockAubFileStream->isOpenCalled);

    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());
    EXPECT_TRUE(mockAubFileStream->getFileNameCalled);

    aubCsr->closeFile();
    EXPECT_FALSE(aubCsr->isFileOpen());
    EXPECT_TRUE(aubCsr->getFileName().empty());
    EXPECT_TRUE(mockAubFileStream->closeCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenFileFunctionsAreCalledThenTheyShouldCallTheExpectedAubManagerFunctions) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";
    aubCsr->aubManager = mockAubManager.get();

    aubCsr->initFile(fileName);
    EXPECT_EQ(1u, mockAubManager->openCalledCnt);

    EXPECT_TRUE(aubCsr->isFileOpen());
    EXPECT_TRUE(mockAubManager->isOpenCalled);

    EXPECT_STREQ(fileName.c_str(), aubCsr->getFileName().c_str());
    EXPECT_TRUE(mockAubManager->getFileNameCalled);

    aubCsr->closeFile();
    EXPECT_FALSE(aubCsr->isFileOpen());
    EXPECT_TRUE(aubCsr->getFileName().empty());
    EXPECT_TRUE(mockAubManager->closeCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenOpenFileIsCalledThenFileStreamShouldBeLocked) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";

    aubCsr->openFile(fileName);

    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenReopenFileIsCalledThenFileStreamShouldBeLocked) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::string fileName = "file_name.aub";

    aubCsr->reopenFile(fileName);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenInitializeEngineIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->initializeEngine();
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenSubmitBatchBufferIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto batchBufferGpuAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;

    aubCsr->initializeEngine();
    aubCsr->lockStreamCalled = false;

    aubCsr->submitBatchBufferAub(batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer, aubCsr->getMemoryBank(batchBuffer.commandBufferAllocation),
                                 aubCsr->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation));
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->initializeEngine();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);

    aubCsr->writeMemory(allocation);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenPollForCompletionIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->latestSentTaskCount = 1;
    aubCsr->pollForCompletionTaskCount = 0;

    aubCsr->pollForCompletion();
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryEqualIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->expectMemoryEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenAddAubCommentIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->addAubComment("comment");
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenDumpAllocationIsCalledThenFileStreamShouldBeLocked) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount};

    aubCsr->dumpAllocation(allocation);
    EXPECT_TRUE(aubCsr->lockStreamCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->aubManager = nullptr;
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(aubCsr->initializeEngineCalled);
    EXPECT_TRUE(aubCsr->writeMemoryCalled);
    EXPECT_TRUE(aubCsr->submitBatchBufferCalled);
    EXPECT_FALSE(aubCsr->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenCallingAddAubCommentThenCallAddCommentOnAubFileStream) {
    DebugManagerStateRestore stateRestore;

    auto aubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->stream = aubFileStream.get();
    aubCsr->aubManager = nullptr;

    const char *comment = "message";
    aubCsr->addAubComment(comment);

    EXPECT_TRUE(aubCsr->addAubCommentCalled);
    EXPECT_LT(0u, aubFileStream->addCommentCalled);
    EXPECT_STREQ(comment, aubFileStream->comments[0].c_str());
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenCallingAddAubCommentThenCallAddCommentOnAubManager) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockAubManager = static_cast<MockAubManager *>(aubCsr.aubManager);
    ASSERT_NE(nullptr, mockAubManager);

    const char *comment = "message";
    aubCsr.addAubComment(comment);

    EXPECT_TRUE(aubCsr.addAubCommentCalled);
    EXPECT_TRUE(mockAubManager->addCommentCalled);
    EXPECT_STREQ(comment, mockAubManager->receivedComments.c_str());
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenCallingInsertAubWaitInstructionThenCallPollForCompletion) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    ASSERT_FALSE(aubCsr->pollForCompletionCalled);
    aubCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);
    EXPECT_TRUE(aubCsr->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenNewTaskSinceLastPollWhenCallingPollForCompletionThenCallRegisterPoll) {
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->hardwareContextController.reset(nullptr);
    aubCsr->stream = aubStream.get();

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 49;
    ASSERT_FALSE(aubStream->registerPollCalled);

    aubCsr->pollForCompletion();
    EXPECT_TRUE(aubStream->registerPollCalled);
    EXPECT_EQ(50u, aubCsr->pollForCompletionTaskCount);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenNoNewTasksSinceLastPollWhenCallingPollForCompletionThenDontCallRegisterPoll) {
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->stream = aubStream.get();

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 50;
    ASSERT_FALSE(aubStream->registerPollCalled);

    aubCsr->pollForCompletion();
    EXPECT_FALSE(aubStream->registerPollCalled);
    EXPECT_EQ(50u, aubCsr->pollForCompletionTaskCount);
}

HWTEST_F(AubFileStreamTests, givenNewTaskSinceLastPollWhenDeletingAubCsrThenCallRegisterPoll) {
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->hardwareContextController.reset(nullptr);
    aubCsr->stream = aubStream.get();

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 49;
    ASSERT_FALSE(aubStream->registerPollCalled);

    aubExecutionEnvironment->commandStreamReceiver.reset();
    EXPECT_TRUE(aubStream->registerPollCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenNoNewTaskSinceLastPollWhenDeletingAubCsrThenDontCallRegisterPoll) {
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->stream = aubStream.get();

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 50;
    ASSERT_FALSE(aubStream->registerPollCalled);

    aubExecutionEnvironment->commandStreamReceiver.reset();
    EXPECT_FALSE(aubStream->registerPollCalled);
}

HWTEST_F(AubFileStreamTests, givenNewTasksAndHardwareContextPresentWhenCallingPollForCompletionThenCallPollForCompletion) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 49;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();
    EXPECT_TRUE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenNoNewTasksAndHardwareContextPresentWhenCallingPollForCompletionThenDontCallPollForCompletion) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 50;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();
    EXPECT_FALSE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenNoNewTasksSinceLastPollWhenCallingExpectMemoryThenDontCallRegisterPoll) {
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->stream = aubStream.get();

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 50;
    ASSERT_FALSE(aubStream->registerPollCalled);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr->expectMemoryNotEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_FALSE(aubStream->registerPollCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenNewTasksSinceLastPollWhenCallingExpectMemoryThenCallRegisterPoll) {
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->hardwareContextController.reset(nullptr);
    aubCsr->stream = aubStream.get();

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 49;
    ASSERT_FALSE(aubStream->registerPollCalled);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr->expectMemoryNotEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubStream->registerPollCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverInSubCaptureModeWhenPollForCompletionIsCalledAndSubCaptureIsEnabledThenItShouldCallRegisterPoll) {
    DebugManagerStateRestore stateRestore;
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->hardwareContextController.reset(nullptr);
    aubCsr->stream = aubStream.get();

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 49;
    ASSERT_FALSE(aubStream->registerPollCalled);

    aubCsr->pollForCompletion();

    EXPECT_TRUE(aubStream->registerPollCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverInSubCaptureModeWhenPollForCompletionIsCalledButSubCaptureIsDisabledThenItShouldntCallRegisterPoll) {
    DebugManagerStateRestore stateRestore;
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    auto aubStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->stream = aubStream.get();

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr->subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr->subCaptureManager->isSubCaptureEnabled());

    aubCsr->latestSentTaskCount = 50;
    aubCsr->pollForCompletionTaskCount = 49;
    ASSERT_FALSE(aubStream->registerPollCalled);

    aubCsr->pollForCompletion();

    EXPECT_FALSE(aubStream->registerPollCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithHardwareContextInSubCaptureModeWhenPollForCompletionIsCalledAndSubCaptureIsEnabledThenItShouldCallPollForCompletionOnHwContext) {
    DebugManagerStateRestore stateRestore;
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    aubSubCaptureManagerMock->checkAndActivateSubCapture("kernelName");
    aubCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_TRUE(aubCsr.subCaptureManager->isSubCaptureEnabled());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 49;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();

    EXPECT_TRUE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithHardwareContextInSubCaptureModeWhenPollForCompletionIsCalledButSubCaptureIsDisabledThenItShouldntCallPollForCompletionOnHwContext) {
    DebugManagerStateRestore stateRestore;
    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto hardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->disableSubCapture();
    aubCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    ASSERT_FALSE(aubCsr.subCaptureManager->isSubCaptureEnabled());

    aubCsr.latestSentTaskCount = 50;
    aubCsr.pollForCompletionTaskCount = 49;
    ASSERT_FALSE(hardwareContext->pollForCompletionCalled);

    aubCsr.pollForCompletion();

    EXPECT_FALSE(hardwareContext->pollForCompletionCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenMakeResidentIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    aubCsr->initializeEngine();
    aubCsr->aubManager = nullptr;

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr->writeMemoryCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryEqualIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr->expectMemoryEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubCsr->expectMemoryEqualCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryNotEqualIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr->expectMemoryNotEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubCsr->expectMemoryNotEqualCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryCompressedIsCalledThenItShouldCallTheExpectedFunctions) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();

    aubCsr->expectMemoryCompressed(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(aubCsr->expectMemoryCompressedCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    aubCsr.initializeTagAllocation();

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 1;

    ResidencyContainer allocationsForResidency;

    aubCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->initializeCalled);
    EXPECT_TRUE(mockHardwareContext->submitCalled);
    EXPECT_FALSE(mockHardwareContext->writeAndSubmitCalled);
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);

    EXPECT_TRUE(aubCsr.writeMemoryWithAubManagerCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledWithZeroSizedBufferThenSubmitIsNotCalledOnHwContext) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    aubCsr.initializeTagAllocation();

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency;

    aubCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(mockHardwareContext->writeAndSubmitCalled);
    EXPECT_FALSE(mockHardwareContext->submitCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenMakeResidentIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    aubCsr.initializeEngine();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    aubCsr.processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr.writeMemoryWithAubManagerCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryEqualIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr.expectMemoryEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(mockHardwareContext->expectMemoryCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryNotEqualIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    aubCsr.expectMemoryNotEqual(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x1000), 0x1000);

    EXPECT_TRUE(mockHardwareContext->expectMemoryCalled);
}

HWTEST_F(AubFileStreamWithoutAubStreamTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenFileStreamShouldBeFlushed) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    aubCsr->stream = mockAubFileStream.get();

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_TRUE(mockAubFileStream->flushCalled);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenExpectMemoryIsCalledThenPageWalkIsCallingStreamsExpectMemory) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr->setupContext(pDevice->commandStreamReceivers[0]->getOsContext());
    aubCsr->hardwareContextController.reset(nullptr);
    aubCsr->stream = mockAubFileStream.get();

    uintptr_t gpuAddress = 0x30000;
    void *sourceAddress = reinterpret_cast<void *>(0x50000);
    auto physicalAddress = aubCsr->ppgtt->map(gpuAddress, MemoryConstants::pageSize, PageTableEntry::presentBit, MemoryBanks::mainBank);

    aubCsr->expectMemoryEqual(reinterpret_cast<void *>(gpuAddress), sourceAddress, MemoryConstants::pageSize);

    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, mockAubFileStream->addressSpaceCapturedFromExpectMemory);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(sourceAddress), mockAubFileStream->memoryCapturedFromExpectMemory);
    EXPECT_EQ(physicalAddress, mockAubFileStream->physAddressCapturedFromExpectMemory);
    EXPECT_EQ(MemoryConstants::pageSize, mockAubFileStream->sizeCapturedFromExpectMemory);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithoutAubManagerWhenExpectMMIOIsCalledThenTheCorrectFunctionIsCalledFromAubFileStream) {
    std::string fileName = "file_name.aub";
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(fileName.c_str(), true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    aubCsr->aubManager = nullptr;
    aubCsr->stream = mockAubFileStream.get();
    aubCsr->setupContext(pDevice->commandStreamReceivers[0]->getOsContext());
    aubCsr->initFile(fileName);

    aubCsr->expectMMIO(5, 10);

    EXPECT_EQ(5u, mockAubFileStream->mmioRegisterFromExpectMMIO);
    EXPECT_EQ(10u, mockAubFileStream->expectedValueFromExpectMMIO);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenExpectMMIOIsCalledThenNoFunctionIsCalledFromAubFileStream) {
    std::string fileName = "file_name.aub";
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(fileName.c_str(), true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    aubCsr->stream = mockAubFileStream.get();
    aubCsr->setupContext(pDevice->commandStreamReceivers[0]->getOsContext());
    aubCsr->initFile(fileName);

    aubCsr->expectMMIO(5, 10);

    EXPECT_NE(5u, mockAubFileStream->mmioRegisterFromExpectMMIO);
    EXPECT_NE(10u, mockAubFileStream->expectedValueFromExpectMMIO);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenInitializeEngineIsCalledThenMemTraceCommentWithDriverVersionIsPutIntoAubStream) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();
    aubCsr->hardwareContextController.reset(nullptr);
    aubCsr->initializeEngine();

    std::string commentWithDriverVersion = "driver version: " + std::string(driverVersion);
    EXPECT_EQ(commentWithDriverVersion, mockAubFileStream->comments[0]);
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenInitFileIsCalledThenMemTraceCommentWithDriverVersionIsPutIntoAubStream) {
    auto mockAubManager = std::make_unique<MockAubManager>();

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, executionEnvironment, 0, deviceBitfield);

    aubCsr->aubManager = mockAubManager.get();
    ASSERT_NE(nullptr, aubCsr->aubManager);

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);

    std::string commentWithDriverVersion = "driver version: " + std::string(driverVersion);
    EXPECT_EQ(mockAubManager->receivedComments, commentWithDriverVersion);

    aubCsr->aubManager = nullptr;
}

HWTEST_F(AddPatchInfoCommentsAubTests, givenAddPatchInfoCommentsCalledWhenNoPatchInfoDataObjectsThenCommentsAreEmpty) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();

    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, mockAubFileStream->comments.size());

    EXPECT_EQ("PatchInfoData\n", mockAubFileStream->comments[0]);
    EXPECT_EQ("AllocationsList\n", mockAubFileStream->comments[1]);
    EXPECT_EQ(2u, mockAubFileStream->addCommentCalled);
}

HWTEST_F(AddPatchInfoCommentsAubTests, givenAddPatchInfoCommentsCalledWhenFirstAddCommentsFailsThenFunctionReturnsFalse) {

    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();
    mockAubFileStream->addCommentResult = false;

    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);
    EXPECT_EQ(1u, mockAubFileStream->addCommentCalled);
}

HWTEST_F(AddPatchInfoCommentsAubTests, givenAddPatchInfoCommentsCalledWhenSecondAddCommentsFailsThenFunctionReturnsFalse) {

    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();

    mockAubFileStream->addCommentResults = {true, false};

    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);
    EXPECT_EQ(2u, mockAubFileStream->addCommentCalled);
}

HWTEST_F(AddPatchInfoCommentsAubTests, givenAddPatchInfoCommentsCalledWhenPatchInfoDataObjectsAddedThenCommentsAreNotEmpty) {

    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();

    PatchInfoData patchInfoData[2] = {{0xAAAAAAAA, 128u, PatchInfoAllocationType::defaultType, 0xBBBBBBBB, 256u, PatchInfoAllocationType::defaultType},
                                      {0xBBBBBBBB, 128u, PatchInfoAllocationType::defaultType, 0xDDDDDDDD, 256u, PatchInfoAllocationType::defaultType}};

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData[0]));
    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData[1]));

    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, mockAubFileStream->comments.size());

    EXPECT_EQ("PatchInfoData", mockAubFileStream->comments[0].substr(0, 13));
    EXPECT_EQ("AllocationsList", mockAubFileStream->comments[1].substr(0, 15));

    std::string line;
    std::istringstream input1;
    input1.str(mockAubFileStream->comments[0]);

    uint32_t lineNo = 0;
    while (std::getline(input1, line)) {
        if (line.substr(0, 13) == "PatchInfoData") {
            continue;
        }
        std::ostringstream ss;
        ss << std::hex << patchInfoData[lineNo].sourceAllocation << ";" << patchInfoData[lineNo].sourceAllocationOffset << ";" << patchInfoData[lineNo].sourceType << ";";
        ss << patchInfoData[lineNo].targetAllocation << ";" << patchInfoData[lineNo].targetAllocationOffset << ";" << patchInfoData[lineNo].targetType << ";";

        EXPECT_EQ(ss.str(), line);
        lineNo++;
    }

    std::vector<std::string> expectedAddresses = {"aaaaaaaa", "bbbbbbbb", "cccccccc", "dddddddd"};
    lineNo = 0;

    std::istringstream input2;
    input2.str(mockAubFileStream->comments[1]);
    while (std::getline(input2, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }
    EXPECT_EQ(2u, mockAubFileStream->addCommentCalled);
}

HWTEST_F(AddPatchInfoCommentsAubTests, givenAddPatchInfoCommentsCalledWhenSourceAllocationIsNullThenDoNotAddToAllocationsList) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();

    PatchInfoData patchInfoData = {0x0, 0u, PatchInfoAllocationType::defaultType, 0xBBBBBBBB, 0u, PatchInfoAllocationType::defaultType};
    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData));

    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, mockAubFileStream->comments.size());

    ASSERT_EQ("PatchInfoData", mockAubFileStream->comments[0].substr(0, 13));
    ASSERT_EQ("AllocationsList", mockAubFileStream->comments[1].substr(0, 15));

    std::string line;
    std::istringstream input;
    input.str(mockAubFileStream->comments[1]);

    std::vector<std::string> expectedAddresses = {"bbbbbbbb"};
    while (std::getline(input, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
    }
    EXPECT_EQ(2u, mockAubFileStream->addCommentCalled);
}

HWTEST_F(AddPatchInfoCommentsAubTests, givenAddPatchInfoCommentsCalledWhenTargetAllocationIsNullThenDoNotAddToAllocationsList) {
    auto mockAubFileStream = std::make_unique<MockAubFileStream>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->stream = mockAubFileStream.get();

    PatchInfoData patchInfoData = {0xAAAAAAAA, 0u, PatchInfoAllocationType::defaultType, 0x0, 0u, PatchInfoAllocationType::defaultType};
    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData));

    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, mockAubFileStream->comments.size());

    ASSERT_EQ("PatchInfoData", mockAubFileStream->comments[0].substr(0, 13));
    ASSERT_EQ("AllocationsList", mockAubFileStream->comments[1].substr(0, 15));

    std::string line;
    std::istringstream input;
    input.str(mockAubFileStream->comments[1]);

    std::vector<std::string> expectedAddresses = {"aaaaaaaa"};
    while (std::getline(input, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
    }
    EXPECT_EQ(2u, mockAubFileStream->addCommentCalled);
}

HWTEST2_F(AubFileStreamTests, givenAubCommandStreamReceiverWhenCreateFullFilePathIsCalledForMultipleDevicesThenFileNameIsExtendedWithSuffixToIndicateMultipleDevices, IsAtMostXeHpcCore) {
    DebugManagerStateRestore stateRestore;

    debugManager.flags.CreateMultipleSubDevices.set(1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", mockRootDeviceIndex);
    EXPECT_EQ(std::string::npos, fullName.find("tx"));

    debugManager.flags.CreateMultipleSubDevices.set(2);
    fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", mockRootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("2tx"));
}

HWTEST_F(AubFileStreamTests, givenDisabledGeneratingPerProcessAubNameAndAubCommandStreamReceiverWhenCreateFullFilePathIsCalledThenFileNameIsExtendedWithRootDeviceIndex) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.GenerateAubFilePerProcessId.set(0);

    uint32_t rootDeviceIndex = 123u;
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("_123_aubfile.aub"));
}

HWTEST_F(AubFileStreamTests, givenEnabledGeneratingAubFilePerProcessIdDebugFlagAndAubCommandStreamReceiverWhenCreateFullFilePathIsCalledThenFileNameIsExtendedRootDeviceIndex) {
    DebugManagerStateRestore stateRestore;

    debugManager.flags.GenerateAubFilePerProcessId.set(1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", 1u);
    std::stringstream strExtendedFileName;
    strExtendedFileName << "_1_aubfile_PID_" << SysCalls::getProcessId() << ".aub";
    EXPECT_NE(std::string::npos, fullName.find(strExtendedFileName.str()));
}

HWTEST_F(AubFileStreamTests, givenAUBDumpCaptureDirPathAndGeneratingAubFilePerProcessIdWhenCreateFullFilePathIsCalledThenFileNameIsExtendedRootDeviceIndex) {
    DebugManagerStateRestore stateRestore;

    std::string expectedDirPath = "/tmp/path";
    debugManager.flags.AUBDumpCaptureDirPath.set(expectedDirPath);
    debugManager.flags.GenerateAubFilePerProcessId.set(1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", 1u);

    std::stringstream expectedPerProcessPart;
    expectedPerProcessPart << "_1_aubfile_PID_" << SysCalls::getProcessId() << ".aub";

    EXPECT_NE(std::string::npos, fullName.find(expectedDirPath));
    EXPECT_NE(std::string::npos, fullName.find(expectedPerProcessPart.str()));
}

HWTEST_F(AubFileStreamTests, givenAndAubCommandStreamReceiverWhenCreateFullFilePathIsCalledThenFileNameIsExtendedRootDeviceIndexAndPid) {
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", 1u);
    std::stringstream strExtendedFileName;
    strExtendedFileName << "_1_aubfile_PID_" << SysCalls::getProcessId() << ".aub";
    EXPECT_NE(std::string::npos, fullName.find(strExtendedFileName.str()));
}

HWTEST_F(AubFileStreamTests, givenAubCommandStreamReceiverWithAubManagerWhenInitFileIsCalledThenCommentWithNonDefaultFlagsAreAdded) {
    DebugManagerStateRestore stateRestore;

    debugManager.flags.MakeAllBuffersResident.set(1);
    debugManager.flags.ZE_AFFINITY_MASK.set("non-default");

    auto mockAubManager = std::make_unique<MockAubManager>();
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    aubCsr->aubManager = mockAubManager.get();

    std::string fileName = "file_name.aub";
    aubCsr->initFile(fileName);

    std::string expectedAddedComments = std::string("driver version: ") + std::string(driverVersion) +
                                        std::string("Non-default value of debug variable: MakeAllBuffersResident = 1") +
                                        std::string("Non-default value of debug variable: ZE_AFFINITY_MASK = non-default");

    EXPECT_EQ(expectedAddedComments, mockAubManager->receivedComments);
}