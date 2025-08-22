#include "RenderBuffer.h"

RenderBuffer::RenderBuffer() {}

RenderBuffer::RenderBuffer(uint32_t capacity, vg::Flags<vg::BufferUsage> bufferUsage)
    : renderingBuffer{vg::Buffer(capacity, bufferUsage), vg::Buffer(capacity, bufferUsage)},
      stagingBuffer(capacity, bufferUsage), size(0), bufferUsage(bufferUsage) {
    vg::Allocate(renderingBuffer[0], {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
    vg::Allocate(renderingBuffer[1], {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
    vg::Allocate(stagingBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
}

RenderBuffer::~RenderBuffer() {}

bool RenderBuffer::FlushBuffer(int index) {
    bufferChangeFlag = 0;
    if (stagingBuffer.GetSize() != renderingBuffer[index].GetSize()) {
        vg::Buffer newBuffer(stagingBuffer.GetSize(), bufferUsage);
        vg::Allocate(newBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});

        std::swap(renderingBuffer[index], newBuffer);
        memcpy(renderingBuffer[index].MapMemory(), stagingBuffer.MapMemory(), stagingBuffer.GetSize());

        return true;
    }
    memcpy(renderingBuffer[index].MapMemory(), stagingBuffer.MapMemory(), stagingBuffer.GetSize());

    return false;
}

vg::Buffer &RenderBuffer::GetBuffer(int index) { return renderingBuffer[index]; }
const vg::Buffer &RenderBuffer::GetBuffer(int index) const { return renderingBuffer[index]; }

uint32_t RenderBuffer::Allocate(uint32_t byteSize, uint32_t alignment) {
    bufferChangeFlag.Set(BufferChange::Contents);
    int currentSize = this->size;
    int padding = (alignment - currentSize % alignment) % alignment;

    size += padding + byteSize;
    if (size >= stagingBuffer.GetSize()) Reserve(size);

    sizes.push_back(byteSize);
    alignments.push_back(alignment);
    offsets.push_back(currentSize + padding);

    return sizes.size() - 1;
}

void RenderBuffer::Reallocate(uint32_t regionID, uint32_t newByteSize) {
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Reallocate()");
    assert(
        newByteSize > 0 &&
        "New byte size must be greater than 0 in RenderBuffer::Reallocate(), to deallocate use Deallocate()"
    );

    int32_t delta = (int32_t)(newByteSize) - (int32_t)(sizes[regionID]);
    if (delta == 0) return;

    std::vector<uint32_t> newOffsets;
    newOffsets.reserve(sizes.size() - regionID);
    uint32_t baseOffset = offsets[regionID] + newByteSize;
    
    for (uint32_t i = regionID + 1; i < offsets.size(); i++) {
        uint32_t padding = GetPadding(i, baseOffset);
        baseOffset += padding;
        newOffsets.push_back(baseOffset);
        baseOffset += sizes[i];
    }

    size = baseOffset;
    if (size > stagingBuffer.GetSize()) Reserve(size);

    char *mem = stagingBuffer.MapMemory();
    if (delta > 0) { // expanding
        for (int i = 0; i < newOffsets.size(); i++) {
            uint32_t regID = offsets.size() - i - 1;
            memmove(mem + newOffsets[newOffsets.size() - i - 1], mem + offsets[regID], sizes[regID]);
            offsets[regID] = newOffsets[newOffsets.size() - i - 1];
        }
    } else { // shrinking
        for (int i = 0; i < newOffsets.size(); i++) {
            memmove(mem + newOffsets[i], mem + offsets[regionID + i + 1], sizes[regionID + i + 1]);
            offsets[regionID + i + 1] = newOffsets[i];
        }
    }

    sizes[regionID] = newByteSize;
    bufferChangeFlag.Set(BufferChange::Contents);
}

void RenderBuffer::Deallocate(uint32_t regionID) { // ma dealokowac caly region
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Deallocate()");
    bufferChangeFlag.Set(BufferChange::Contents);

    uint32_t writeOffset = offsets[regionID];
    uint32_t removedSize = sizes[regionID];

    for (uint32_t i = regionID + 1; i < offsets.size(); i++) {
        uint32_t padding = GetPadding(i, writeOffset);
        writeOffset += padding;

        memcpy(stagingBuffer.MapMemory() + writeOffset, stagingBuffer.MapMemory() + offsets[i], sizes[i]);

        offsets[i] = writeOffset;
        writeOffset += sizes[i];
    }
    sizes.erase(sizes.begin() + regionID);
    offsets.erase(offsets.begin() + regionID);
    alignments.erase(alignments.begin() + regionID);

    // Korekta offsetów
    for (uint32_t i = regionID; i < offsets.size(); ++i) {
        if (i == 0) { continue; }
        offsets[i] = offsets[i - 1] + sizes[i - 1] + GetPadding(i, offsets[i - 1] + sizes[i - 1]);
    }
}

void RenderBuffer::Reserve(uint32_t capacity) {
    if (capacity <= stagingBuffer.GetSize()) return;
    bufferChangeFlag.Set(BufferChange::Size);

    vg::Buffer newBuffer(capacity, bufferUsage);
    vg::Allocate(newBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    memcpy(newBuffer.MapMemory(), stagingBuffer.MapMemory(), stagingBuffer.GetSize());

    std::swap(stagingBuffer, newBuffer);
}

void RenderBuffer::Erase(
    uint32_t regionID, uint32_t eraseSize, uint32_t eraseOffset
) 
{
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Erase()");
    assert(
        eraseOffset + eraseSize <= sizes[regionID] &&
        "eraseOffset + eraseSize exceeds region size in RenderBuffer::Erase()"
    );
    assert(eraseSize > 0 && "eraseSize must be greater than 0 in RenderBuffer::Erase()");
    bufferChangeFlag.Set(BufferChange::Contents); // do czego to jest nwm

    uint32_t regionOffset = offsets[regionID];
    uint32_t regionSize = sizes[regionID];

    if (eraseOffset + eraseSize < regionSize) {
        uint32_t sourceOffset = regionOffset + eraseOffset + eraseSize;
        uint32_t destOffset = regionOffset + eraseOffset;
        uint32_t moveSize = regionSize - (eraseOffset + eraseSize);

        memmove(stagingBuffer.MapMemory() + destOffset, stagingBuffer.MapMemory() + sourceOffset, moveSize);
    }

    sizes[regionID] -= eraseSize;

    uint32_t baseOffset = offsets[regionID] + sizes[regionID];
    for (uint32_t i = regionID + 1; i < offsets.size(); i++) {
        uint32_t padding = GetPadding(i, baseOffset);
        baseOffset += padding;
        
        // Przenieś dane regionu na nową pozycję
        memmove(stagingBuffer.MapMemory() + baseOffset, stagingBuffer.MapMemory() + offsets[i], sizes[i]);
        
        // Aktualizuj offset regionu
        offsets[i] = baseOffset;
        baseOffset += sizes[i];
    }
}

void RenderBuffer::Write(uint32_t regionID, const void *data, uint32_t dataSize, uint32_t writeOffset) {
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Write()");
    assert(data && "Null data pointer in RenderBuffer::Write()");
    assert(
        dataSize + writeOffset <= sizes[regionID] &&
        "dataSize + writeOffset larger than region size in RenderBuffer::Write()"
    );
    bufferChangeFlag.Set(BufferChange::Contents);
    memcpy(stagingBuffer.MapMemory() + offsets[regionID] + writeOffset, data, dataSize);
}

void RenderBuffer::Write(uint32_t regionID, const void *data) { Write(regionID, data, sizes[regionID]); }

uint32_t RenderBuffer::Read(uint32_t regionID, void *data, uint32_t maximumReadSize, uint32_t readOffset) {
    assert(regionID < sizes.size() && "Invalid regionID");
    assert(readOffset < sizes[regionID] && "Read offset larger than region size");

    maximumReadSize = std::min(maximumReadSize, sizes[regionID] - readOffset);

    if (data) memcpy(data, stagingBuffer.MapMemory() + offsets[regionID] + readOffset, maximumReadSize);

    return maximumReadSize;
}

int RenderBuffer::GetCapacity() const { return stagingBuffer.GetSize(); }

int RenderBuffer::GetSize() const { return size; }

vg::Flags<BufferChange> RenderBuffer::GetChange() const { return bufferChangeFlag; }

uint32_t RenderBuffer::Size(uint32_t regionID) const {
    assert(regionID < sizes.size() && "Invalid regionID in Size()");
    return sizes[regionID];
}

uint32_t RenderBuffer::Alignment(uint32_t regionID) const {
    assert(regionID < alignments.size() && "Invalid regionID in Alignment()");
    return alignments[regionID];
}

uint32_t RenderBuffer::Offset(uint32_t regionID) const {
    assert(regionID < offsets.size() && "Invalid regionID in Offset()");
    return offsets[regionID];
}

uint32_t RenderBuffer::GetPadding(uint32_t regionID, uint32_t offset) const {
    assert(regionID < alignments.size() && "Invalid regionID in GetPadding()");
    return (alignments[regionID] - offset % alignments[regionID]) % alignments[regionID];
}
