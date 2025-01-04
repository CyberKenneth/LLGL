/*
 * D3D12StagingBufferPool.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include "D3D12StagingBufferPool.h"
#include "../Command/D3D12CommandContext.h"
#include "../Command/D3D12CommandQueue.h"
#include "../D3D12Resource.h"
#include "../../../Core/CoreUtils.h"
#include <algorithm>


namespace LLGL
{


D3D12StagingBufferPool::D3D12StagingBufferPool(ID3D12Device* device, UINT64 chunkSize) :
    device_    { device    },
    chunkSize_ { chunkSize }
{
}

void D3D12StagingBufferPool::InitializeDevice(ID3D12Device* device, UINT64 chunkSize)
{
    device_    = device;
    chunkSize_ = chunkSize;
}

void D3D12StagingBufferPool::Reset()
{
    for (D3D12StagingBuffer& chunk : chunks_)
        chunk.Reset();
    chunkIdx_ = 0;
}

HRESULT D3D12StagingBufferPool::WriteStaged(
    D3D12CommandContext&    commandContext,
    D3D12Resource&          dstBuffer,
    UINT64                  dstOffset,
    const void*             data,
    UINT64                  dataSize)
{
    /* Find a chunk that fits the requested data size or allocate a new chunk */
    while (chunkIdx_ < chunks_.size() && !chunks_[chunkIdx_].Capacity(dataSize))
        ++chunkIdx_;

    if (chunkIdx_ == chunks_.size())
        AllocChunk(dataSize);

    /* Write data to current chunk */
    HRESULT hr;
    const D3D12_RESOURCE_STATES oldResourceState = dstBuffer.currentState;
    commandContext.TransitionResource(dstBuffer, D3D12_RESOURCE_STATE_COPY_DEST, true);
    {
        D3D12StagingBuffer& chunk = chunks_[chunkIdx_];
        hr = chunk.WriteAndIncrementOffset(commandContext.GetCommandList(), dstBuffer.Get(), dstOffset, data, dataSize);
    }
    commandContext.TransitionResource(dstBuffer, oldResourceState);
    return hr;
}

HRESULT D3D12StagingBufferPool::WriteImmediate(
    D3D12CommandContext&    commandContext,
    D3D12Resource&          dstBuffer,
    UINT64                  dstOffset,
    const void*             data,
    UINT64                  dataSize,
    UINT64                  alignment)
{
    /* Write data to global upload buffer and copy region to destination buffer */
    HRESULT hr;
    const D3D12_RESOURCE_STATES oldResourceState = dstBuffer.currentState;
    commandContext.TransitionResource(dstBuffer, D3D12_RESOURCE_STATE_COPY_DEST, true);
    {
        hr = GetUploadBufferAndGrow(dataSize, alignment).Write(commandContext.GetCommandList(), dstBuffer.Get(), dstOffset, data, dataSize);
    }
    commandContext.TransitionResource(dstBuffer, oldResourceState);
    return hr;
}

HRESULT D3D12StagingBufferPool::ReadSubresourceRegion(
    D3D12CommandContext&    commandContext,
    D3D12CommandQueue&      commandQueue,
    D3D12Resource&          srcBuffer,
    UINT64                  srcOffset,
    void*                   data,
    UINT64                  dataSize,
    UINT64                  alignment)
{
    D3D12StagingBuffer& readbackBuffer = GetReadbackBufferAndGrow(dataSize, alignment);

    /* Copy source buffer region to readback buffer and flush command list */
    const D3D12_RESOURCE_STATES oldResourceState = srcBuffer.currentState;
    commandContext.TransitionResource(srcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);
    {
        commandContext.GetCommandList()->CopyBufferRegion(readbackBuffer.GetNative(), 0, srcBuffer.Get(), srcOffset, dataSize);
    }
    commandContext.TransitionResource(srcBuffer, oldResourceState);
    commandQueue.FinishAndSubmitCommandContext(commandContext, true);

    /* Map readback buffer to CPU memory space */
    char* mappedData = nullptr;
    const D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(dataSize) };

    HRESULT hr = readbackBuffer.GetNative()->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr))
        return hr;

    /* Copy readback buffer into output data */
    ::memcpy(data, mappedData, static_cast<std::size_t>(dataSize));

    /* Unmap buffer with range of written data */
    const D3D12_RANGE writtenRange{ 0, 0 };
    readbackBuffer.GetNative()->Unmap(0, &writtenRange);

    return S_OK;
}

HRESULT D3D12StagingBufferPool::MapFeedbackBuffer(
    D3D12CommandContext&    commandContext,
    D3D12CommandQueue&      commandQueue,
    D3D12Resource&          srcBuffer,
    const D3D12_RANGE&      readRange,
    void**                  mappedData)
{
    /* Copy content from GPU host memory to CPU memory */
    UINT64 numBytes = readRange.End - readRange.Begin;
    D3D12StagingBuffer& readbackBuffer = GetReadbackBufferAndGrow(numBytes);

    commandContext.TransitionResource(srcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);

    commandContext.GetCommandList()->CopyBufferRegion(
        readbackBuffer.GetNative(),
        0,
        srcBuffer.Get(),
        readRange.Begin,
        numBytes
    );

    commandQueue.FinishAndSubmitCommandContext(commandContext, true);

    /* Map with read range */
    const D3D12_RANGE cpuAccessRange{ 0, static_cast<SIZE_T>(numBytes) };
    return readbackBuffer.GetNative()->Map(0, &cpuAccessRange, mappedData);
}

void D3D12StagingBufferPool::UnmapFeedbackBuffer()
{
    /* Unmap without written range */
    const D3D12_RANGE nullRange{ 0, 0 };
    globalReadbackBuffer_.GetNative()->Unmap(0, &nullRange);
}

HRESULT D3D12StagingBufferPool::MapUploadBuffer(
    SIZE_T                  size,
    void**                  mappedData)
{
    /* Map without read range */
    D3D12StagingBuffer& uploadBuffer = GetUploadBufferAndGrow(size);
    const D3D12_RANGE nullRange{ 0, 0, };
    return uploadBuffer.GetNative()->Map(0, &nullRange, mappedData);
}

void D3D12StagingBufferPool::UnmapUploadBuffer(
    D3D12CommandContext&    commandContext,
    D3D12CommandQueue&      commandQueue,
    D3D12Resource&          dstBuffer,
    const D3D12_RANGE&      writtenRange)
{
    /* Unmap with written range */
    globalUploadBuffer_.GetNative()->Unmap(0, &writtenRange);

    /* Copy content from CPU memory to GPU host memory */
    commandContext.TransitionResource(dstBuffer, D3D12_RESOURCE_STATE_COPY_DEST, true);

    commandContext.GetCommandList()->CopyBufferRegion(
        dstBuffer.Get(),
        writtenRange.Begin,
        globalUploadBuffer_.GetNative(),
        0,
        writtenRange.End - writtenRange.Begin
    );

    commandQueue.FinishAndSubmitCommandContext(commandContext, true);
}


/*
 * ======= Private: =======
 */

void D3D12StagingBufferPool::AllocChunk(UINT64 minChunkSize)
{
    chunks_.emplace_back(device_, std::max(chunkSize_, minChunkSize));
    chunkIdx_ = chunks_.size() - 1;
}

void D3D12StagingBufferPool::ResizeBuffer(
    D3D12StagingBuffer& stagingBuffer,
    D3D12_HEAP_TYPE     heapType,
    UINT64              size,
    UINT64              alignment)
{
    /* Check if global upload buffer must be resized */
    UINT64 alignedSize = GetAlignedSize(size, alignment);
    if (!stagingBuffer.Capacity(alignedSize))
    {
        /* Use at least a bigger alignment for allocating the global buffers to reduce number of reallocations */
        constexpr UINT64 minAlignment = 4096ull;
        stagingBuffer.Create(device_, alignedSize, minAlignment, heapType);
    }
}

D3D12StagingBuffer& D3D12StagingBufferPool::GetUploadBufferAndGrow(UINT64 size, UINT64 alignment)
{
    ResizeBuffer(globalUploadBuffer_, D3D12_HEAP_TYPE_UPLOAD, size, alignment);
    return globalUploadBuffer_;
}

D3D12StagingBuffer& D3D12StagingBufferPool::GetReadbackBufferAndGrow(UINT64 size, UINT64 alignment)
{
    ResizeBuffer(globalReadbackBuffer_, D3D12_HEAP_TYPE_READBACK, size, alignment);
    return globalReadbackBuffer_;
}


} // /namespace LLGL



// ================================================================================
