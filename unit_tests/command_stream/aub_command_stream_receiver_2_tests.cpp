/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_csr.h"
#include "unit_tests/mocks/mock_aub_file_stream.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"

using namespace OCLRT;

typedef Test<DeviceFixture> AubCommandStreamReceiverTests;

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneAndSubCaptureModeWhenSubCaptureGetsDeactivatedThenCsrIsFlushed) {
    DebugManagerStateRestore stateRestore;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));

    auto subCaptureManagerMock = new AubSubCaptureManagerMock("");
    subCaptureManagerMock->subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    subCaptureManagerMock->setSubCaptureIsActive(true);
    subCaptureManagerMock->setSubCaptureToggleActive(false);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    aubCsr->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(aubCsr->flushBatchedSubmissionsCalled);
    EXPECT_FALSE(aubCsr->initProgrammingFlagsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInImmediateDispatchModeThenNewCombinedBatchBufferIsCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer->getUnderlyingBuffer());
    EXPECT_EQ(alignUp(128u + 128u, 0x1000), sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferInImmediateDispatchModeAndNoChainedBatchBufferThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferAndNotImmediateOrBatchedDispatchModeThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::AdaptiveDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRegisterCommandChunkIsCalledThenNewChunkIsAddedToTheList) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(batchBuffer, sizeof(MI_BATCH_BUFFER_START));
    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(128u + sizeof(MI_BATCH_BUFFER_START), aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[0].endOffset);

    CommandChunk chunk;
    chunk.endOffset = 0x123;
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk);

    ASSERT_EQ(2u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(0x123u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[1].endOffset);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRemovePatchInfoDataIsCalledThenElementIsRemovedFromPatchInfoList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    PatchInfoData patchInfoData(0xA000, 0x0, PatchInfoAllocationType::KernelArg, 0xB000, 0x0, PatchInfoAllocationType::Default);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData);
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xC000));
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xB000));
    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddGucStartMessageIsCalledThenBatchBufferAddressIsStoredInPatchInfoCollection) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    std::unique_ptr<char> batchBuffer(new char[1024]);
    aubCsr->addGUCStartMessage(static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(batchBuffer.get())), EngineType::ENGINE_RCS);

    auto &patchInfoCollection = aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection();
    ASSERT_EQ(1u, patchInfoCollection.size());
    EXPECT_EQ(patchInfoCollection[0].sourceAllocation, reinterpret_cast<uint64_t>(batchBuffer.get()));
    EXPECT_EQ(patchInfoCollection[0].targetType, PatchInfoAllocationType::GUCStartMessage);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInBatchedDispatchModeThenNewCombinedBatchBufferIsCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    CommandChunk chunk1;
    CommandChunk chunk2;
    CommandChunk chunk3;

    std::unique_ptr<char> commands1(new char[0x100u]);
    commands1.get()[0] = 0x1;
    chunk1.baseAddressCpu = chunk1.baseAddressGpu = reinterpret_cast<uint64_t>(commands1.get());
    chunk1.startOffset = 0u;
    chunk1.endOffset = 0x50u;

    std::unique_ptr<char> commands2(new char[0x100u]);
    commands2.get()[0] = 0x2;
    chunk2.baseAddressCpu = chunk2.baseAddressGpu = reinterpret_cast<uint64_t>(commands2.get());
    chunk2.startOffset = 0u;
    chunk2.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands2.get() + 0x40), reinterpret_cast<uint64_t>(commands1.get()));

    std::unique_ptr<char> commands3(new char[0x100u]);
    commands3.get()[0] = 0x3;
    chunk3.baseAddressCpu = chunk3.baseAddressGpu = reinterpret_cast<uint64_t>(commands3.get());
    chunk3.startOffset = 0u;
    chunk3.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands3.get() + 0x40), reinterpret_cast<uint64_t>(commands2.get()));

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk1);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk2);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    PatchInfoData patchInfoData1(0xAAAu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, chunk1.baseAddressGpu, 0x10, PatchInfoAllocationType::Default);
    PatchInfoData patchInfoData2(0xBBBu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, chunk1.baseAddressGpu, 0x60, PatchInfoAllocationType::Default);
    PatchInfoData patchInfoData3(0xCCCu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, 0x0, 0x10, PatchInfoAllocationType::Default);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0u;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::BatchedDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });

    EXPECT_NE(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(alignUp(0x50u + 0x40u + 0x40u + CSRequirements::csOverfetchSize, 0x1000u), sizeBatchBuffer);

    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
    EXPECT_EQ(0xAAAu, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection()[0].sourceAllocation);

    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    EXPECT_EQ(0x3, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0]);
    EXPECT_EQ(0x2, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0x40]);
    EXPECT_EQ(0x1, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0x40 + 0x40]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenDefaultDebugConfigThenExpectFlattenBatchBufferIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(0);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeThenExpectFlattenBatchBufferIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    auto chainedBatchBuffer = aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> ptr(
        aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemory(4096, 4096, false, false),
        [&](GraphicsAllocation *ptr) { aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(ptr); });

    auto expectedAllocation = ptr.get();
    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(ptr.release()));
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_EQ(batchBuffer.commandBufferAllocation, expectedAllocation);

    aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(chainedBatchBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeAndThereIsNoChainedBatchBufferThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndBatchedDispatchModeThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);
    ResidencyContainer allocationsForResidency;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoCommentsIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(1);
    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoCommentsIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(0);

    aubCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForEmptyPatchInfoListThenIndirectPatchCommandBufferIsNotCreated) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(0u, indirectPatchCommandsSize);
    EXPECT_EQ(0u, indirectPatchInfo.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForNonEmptyPatchInfoListThenIndirectPatchCommandBufferIsCreated) {
    typedef typename FamilyType::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    PatchInfoData patchInfo1(0xA000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x100, PatchInfoAllocationType::IndirectObjectHeap);
    PatchInfoData patchInfo2(0xB000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x200, PatchInfoAllocationType::IndirectObjectHeap);
    PatchInfoData patchInfo3(0xC000, 0u, PatchInfoAllocationType::IndirectObjectHeap, 0x1000, 0x100, PatchInfoAllocationType::Default);
    PatchInfoData patchInfo4(0xC000, 0u, PatchInfoAllocationType::Default, 0x2000, 0x100, PatchInfoAllocationType::GUCStartMessage);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo3);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo4);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(4u, indirectPatchInfo.size());
    EXPECT_EQ(2u * sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddBatchBufferStartCalledAndBatchBUfferFlatteningEnabledThenBatchBufferStartAddressIsRegistered) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, *pDevice->executionEnvironment));
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    MI_BATCH_BUFFER_START bbStart;

    aubCsr->addBatchBufferStart(&bbStart, 0xA000u, false);
    std::map<uint64_t, uint64_t> &batchBufferStartAddressSequence = aubCsr->getFlatBatchBufferHelper().getBatchBufferStartAddressSequence();

    ASSERT_EQ(1u, batchBufferStartAddressSequence.size());
    std::pair<uint64_t, uint64_t> addr = *batchBufferStartAddressSequence.begin();
    EXPECT_EQ(reinterpret_cast<uint64_t>(&bbStart), addr.first);
    EXPECT_EQ(0xA000u, addr.second);
}

class OsAgnosticMemoryManagerForImagesWithNoHostPtr : public OsAgnosticMemoryManager {
  public:
    OsAgnosticMemoryManagerForImagesWithNoHostPtr(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, false, executionEnvironment) {}

    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override {
        auto imageAllocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
        cpuPtr = imageAllocation->getUnderlyingBuffer();
        imageAllocation->setCpuPtrAndGpuAddress(nullptr, imageAllocation->getGpuAddress());
        return imageAllocation;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *imageAllocation) override {
        imageAllocation->setCpuPtrAndGpuAddress(cpuPtr, imageAllocation->getGpuAddress());
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(imageAllocation);
    };
    void *lockResource(GraphicsAllocation *imageAllocation) override {
        lockResourceParam.wasCalled = true;
        lockResourceParam.inImageAllocation = imageAllocation;
        lockCpuPtr = alignedMalloc(imageAllocation->getUnderlyingBufferSize(), MemoryConstants::pageSize);
        lockResourceParam.retCpuPtr = lockCpuPtr;
        return lockResourceParam.retCpuPtr;
    };
    void unlockResource(GraphicsAllocation *imageAllocation) override {
        unlockResourceParam.wasCalled = true;
        unlockResourceParam.inImageAllocation = imageAllocation;
        alignedFree(lockCpuPtr);
    };

    struct LockResourceParam {
        bool wasCalled = false;
        GraphicsAllocation *inImageAllocation = nullptr;
        void *retCpuPtr = nullptr;
    } lockResourceParam;
    struct UnlockResourceParam {
        bool wasCalled = false;
        GraphicsAllocation *inImageAllocation = nullptr;
    } unlockResourceParam;

  protected:
    void *cpuPtr = nullptr;
    void *lockCpuPtr = nullptr;
};

using AubCommandStreamReceiverNoHostPtrTests = ::testing::Test;
HWTEST_F(AubCommandStreamReceiverNoHostPtrTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnImageWithNoHostPtrThenResourceShouldBeLockedToGetCpuAddress) {
    ExecutionEnvironment executionEnvironment;
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], "", true, executionEnvironment));

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 512;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    auto imageAllocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    ASSERT_NE(nullptr, imageAllocation);

    EXPECT_TRUE(aubCsr->writeMemory(*imageAllocation));

    EXPECT_TRUE(memoryManager->lockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->lockResourceParam.inImageAllocation);
    EXPECT_NE(nullptr, memoryManager->lockResourceParam.retCpuPtr);

    EXPECT_TRUE(memoryManager->unlockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->unlockResourceParam.inImageAllocation);

    queryGmm.release();
    memoryManager->freeGraphicsMemory(imageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenNoDbgDeviceIdFlagWhenAubCsrIsCreatedThenUseDefaultDeviceId) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(hwInfoIn.capabilityTable.aubDeviceId, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenDbgDeviceIdFlagIsSetWhenAubCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    EXPECT_EQ(9u, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGTTDataIsCalledThenLocalMemoryIsSetAccordingToCsrFeature) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    AubGTTData data = {};
    aubCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.present);

    if (aubCsr->localMemoryEnabled) {
        EXPECT_TRUE(data.localMemory);
    } else {
        EXPECT_FALSE(data.localMemory);
    }
}

HWTEST_F(AubCommandStreamReceiverTests, givenPhysicalAddressWhenSetGttEntryIsCalledThenGttEntrysBitFieldsShouldBePopulated) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;

    AubMemDump::MiGttEntry entry = {};
    uint64_t address = 0x0123456789;
    AubGTTData data = {true, false};
    AUB::setGttEntry(entry, address, data);

    EXPECT_EQ(entry.pageConfig.PhysicalAddress, address / 4096);
    EXPECT_TRUE(entry.pageConfig.Present);
    EXPECT_FALSE(entry.pageConfig.LocalMemory);
}

HWTEST_F(AubCommandStreamReceiverTests, whenGetMemoryBankForGttIsCalledThenCorrectBankIsReturned) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));
    aubCsr->localMemoryEnabled = false;

    auto bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::MainBank, bank);
}

HWTEST_F(AubCommandStreamReceiverTests, givenEntryBitsPresentAndWritableWhenGetAddressSpaceFromPTEBitsIsCalledThenTraceNonLocalIsReturned) {
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(hwInfoIn, "", true, *pDevice->executionEnvironment));

    auto space = aubCsr->getAddressSpaceFromPTEBits(PageTableEntry::presentBit | PageTableEntry::writableBit);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, space);
}

template <typename GfxFamily>
struct MockAubCsrToTestExternalAllocations : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::externalAllocations;

    bool writeMemory(AllocationView &allocationView) override {
        writeMemoryParametrization.wasCalled = true;
        writeMemoryParametrization.receivedAllocationView = allocationView;
        writeMemoryParametrization.statusToReturn = (0 != allocationView.second) ? true : false;
        return writeMemoryParametrization.statusToReturn;
    }
    struct WriteMemoryParametrization {
        bool wasCalled = false;
        AllocationView receivedAllocationView = {};
        bool statusToReturn = false;
    } writeMemoryParametrization;
};

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeResidentExternalIsCalledThenGivenAllocationViewShouldBeAddedToExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);

    ASSERT_EQ(0u, aubCsr->externalAllocations.size());
    aubCsr->makeResidentExternal(externalAllocation);
    EXPECT_EQ(1u, aubCsr->externalAllocations.size());
    EXPECT_EQ(addr, aubCsr->externalAllocations[0].first);
    EXPECT_EQ(size, aubCsr->externalAllocations[0].second);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeNonResidentExternalIsCalledThenMatchingAllocationViewShouldBeRemovedFromExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    aubCsr->makeNonResidentExternal(addr);
    EXPECT_EQ(0u, aubCsr->externalAllocations.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeNonResidentExternalIsCalledThenNonMatchingAllocationViewShouldNotBeRemovedFromExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    aubCsr->makeNonResidentExternal(0);
    EXPECT_EQ(1u, aubCsr->externalAllocations.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationsShouldBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(addr, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(size, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_TRUE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationWithZeroSizeShouldNotBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>(*platformDevices[0], "", true, *pDevice->executionEnvironment);
    AllocationView externalAllocation(0, 0);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_FALSE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledThenGraphicsAllocationSizeIsReadCorrectly) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", false, *pDevice->executionEnvironment);
    memoryManager.reset(aubCsr->createMemoryManager(false, false));

    PhysicalAddressAllocator allocator;
    struct PpgttMock : std::conditional<is64bit, PML4, PDPE>::type {
        PpgttMock(PhysicalAddressAllocator *allocator) : std::conditional<is64bit, PML4, PDPE>::type(allocator) {}

        void pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank) override {
            receivedSize = size;
        }
        size_t receivedSize = 0;
    };
    auto ppgttMock = new PpgttMock(&allocator);

    aubCsr->ppgtt.reset(ppgttMock);

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxAllocation->setAubWritable(true);

    auto gmm = new Gmm(nullptr, 1, false);
    gfxAllocation->gmm = gmm;

    for (bool compressed : {false, true}) {
        gmm->isRenderCompressed = compressed;

        aubCsr->writeMemory(*gfxAllocation);

        if (compressed) {
            EXPECT_EQ(gfxAllocation->gmm->gmmResourceInfo->getSizeAllocation(), ppgttMock->receivedSize);
        } else {
            EXPECT_EQ(gfxAllocation->getUnderlyingBufferSize(), ppgttMock->receivedSize);
        }
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, whenAubCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(*platformDevices[0], "", false, *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, aubCsr->ppgtt.get());
    ASSERT_NE(nullptr, aubCsr->ggtt.get());

    uintptr_t address = 0x20000;
    auto physicalAddress = aubCsr->ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = aubCsr->ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenEngineIsInitializedThenDumpHandleIsGenerated) {
    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto engineType = OCLRT::ENGINE_RCS;
    auto engineIndex = aubCsr->getEngineIndex(engineType);

    aubCsr->initializeEngine(engineIndex);
    EXPECT_NE(0u, aubCsr->handle);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddMmioKeySetToZeroWhenInitAdditionalMmioCalledThenDoNotWriteMmio) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set("");

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(0u, stream->mmioList.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddMmioRegistersListSetWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("0xdead;0xbeef");
    MMIOPair mmioPair(0xdead, 0xbeef);

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(1u, stream->mmioList.size());
    EXPECT_TRUE(stream->isOnMmioList(mmioPair));
};

HWTEST_F(AubCommandStreamReceiverTests, givenLongSequenceOfAddMmioRegistersListSetWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("1;1;2;2;3;3");

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(3u, stream->mmioList.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenSequenceWithIncompletePairOfAddMmioRegistersListSetWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("0x1;0x1;0x2");
    MMIOPair mmioPair0(0x1, 0x1);
    MMIOPair mmioPair1(0x2, 0x2);

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(1u, stream->mmioList.size());
    EXPECT_TRUE(stream->isOnMmioList(mmioPair0));
    EXPECT_FALSE(stream->isOnMmioList(mmioPair1));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddMmioRegistersListSetWithSemicolonAtTheEndWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("0xdead;0xbeef;");
    MMIOPair mmioPair(0xdead, 0xbeef);

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(1u, stream->mmioList.size());
    EXPECT_TRUE(stream->isOnMmioList(mmioPair));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddMmioRegistersListSetWithInvalidValueWhenInitAdditionalMmioCalledThenMmioIsNotWritten) {
    std::string registers("0xdead;invalid");

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>(**platformDevices, "", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(0u, stream->mmioList.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenAskedForMemoryExpectationThenPassValidCompareOperationType) {
    class MyMockAubCsr : public AUBCommandStreamReceiverHw<FamilyType> {
      public:
        using AUBCommandStreamReceiverHw<FamilyType>::AUBCommandStreamReceiverHw;

        void expectMemory(void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override {
            inputCompareOperation = compareOperation;
            AUBCommandStreamReceiverHw<FamilyType>::expectMemory(gfxAddress, srcAddress, length, compareOperation);
        }
        uint32_t inputCompareOperation = 0;
    };
    void *mockAddress = reinterpret_cast<void *>(1);
    uint32_t compareNotEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual;
    uint32_t compareEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;

    MyMockAubCsr myMockCsr(**platformDevices, std::string(), true, *pDevice->getExecutionEnvironment());
    auto mockStream = std::make_unique<MockAubFileStream>();
    myMockCsr.stream = mockStream.get();

    myMockCsr.expectMemoryNotEqual(mockAddress, mockAddress, 1);
    EXPECT_EQ(compareNotEqual, myMockCsr.inputCompareOperation);
    EXPECT_EQ(compareNotEqual, mockStream->compareOperationFromExpectMemory);

    myMockCsr.expectMemoryEqual(mockAddress, mockAddress, 1);
    EXPECT_EQ(compareEqual, myMockCsr.inputCompareOperation);
    EXPECT_EQ(compareEqual, mockStream->compareOperationFromExpectMemory);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenObtainingPreferredTagPoolSizeThenReturnOne) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>(**platformDevices, "", true, *pDevice->executionEnvironment);
    EXPECT_EQ(1u, aubCsr->getPreferredTagPoolSize());
}