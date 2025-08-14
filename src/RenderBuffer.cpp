#include "RenderBuffer.h"

bool RenderBuffer::Swap() {
    std::swap(frontBuffer, backBuffer);

    if (bufferChangeFlag.IsSet(BufferChange::Size)) {
        backBuffer = vg::Buffer(frontBuffer.GetSize(), bufferUsage);
        vg::Allocate(backBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});

        memcpy(backBuffer.MapMemory(), frontBuffer.MapMemory(), frontBuffer.GetSize());
        bufferChangeFlag = 0;

        return true;
    }

    if (bufferChangeFlag.IsSet(BufferChange::Contents))
        memcpy(backBuffer.MapMemory(), frontBuffer.MapMemory(), frontBuffer.GetSize());

    bufferChangeFlag = 0;

    return false;
}

RenderBuffer::RenderBuffer() {}

RenderBuffer::RenderBuffer(uint32_t capacity, vg::Flags<vg::BufferUsage> bufferUsage)
    : frontBuffer(capacity, bufferUsage), backBuffer(capacity, bufferUsage), size(0), bufferUsage(bufferUsage) {
    vg::Allocate(frontBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
    vg::Allocate(backBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
}

RenderBuffer::~RenderBuffer() {}

RenderBuffer::operator vg::Buffer &() { return frontBuffer; }

RenderBuffer::operator const vg::Buffer &() const { return frontBuffer; }

uint32_t RenderBuffer::Allocate(uint32_t byteSize, uint32_t alignment) {
    bufferChangeFlag.Set(BufferChange::Contents);
    int currentSize = this->size;
    int padding = (alignment - currentSize % alignment) % alignment;

    size += padding + byteSize;
    if (size >= backBuffer.GetSize()) Reserve(size);

    sizes.push_back(byteSize);
    alignments.push_back(alignment);
    offsets.push_back(currentSize + padding);

    return sizes.size() - 1;
}

uint32_t RenderBuffer::Reallocate(uint32_t regionID, uint32_t newByteSize) {
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Reallocate()");
    assert (newByteSize > 0 && "New byte size must be greater than 0 in RenderBuffer::Reallocate(), to deallocate use Deallocate()");

    if (regionID == offsets.size()) {
        Deallocate(regionID);
        return Allocate(newByteSize, alignments[regionID]);
    }
    uint32_t oldSize = sizes[regionID];
    int32_t delta = static_cast<int32_t>(newByteSize) - static_cast<int32_t>(oldSize);

    if (delta == 0) return regionID; // No change needed.

    char* mem = backBuffer.MapMemory();

    if (delta > 0) { // expanding
        for (int i = static_cast<int>(sizes.size()) - 1; i > static_cast<int>(regionID); i--) {
            uint32_t srcOffset = offsets[i];
            uint32_t dstOffset = srcOffset + delta;
            memmove(mem + dstOffset, mem + srcOffset, sizes[i]);
            offsets[i] = dstOffset;
        }
    } else { // shrinking
        for (uint32_t i = regionID + 1; i < offsets.size(); i++) {
            uint32_t srcOffset = offsets[i];
            uint32_t dstOffset = srcOffset + delta;
            memmove(mem + dstOffset, mem + srcOffset, sizes[i]);
            offsets[i] = dstOffset;
        }
    }

    sizes[regionID] = newByteSize;
    size = static_cast<uint32_t>(static_cast<int32_t>(size) + delta);

    bufferChangeFlag.Set(BufferChange::Contents);
    return regionID;
}

void RenderBuffer::Deallocate(uint32_t regionID) { // ma dealokowac caly region 
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Deallocate()");
    bufferChangeFlag.Set(BufferChange::Contents);

    uint32_t writeOffset = offsets[regionID];
    uint32_t removedSize = sizes[regionID];
    
    for (uint32_t i = regionID + 1; i < offsets.size(); i++) {
        uint32_t padding = GetPadding(i, writeOffset);
        writeOffset += padding;

        memcpy(backBuffer.MapMemory() + writeOffset, backBuffer.MapMemory() + offsets[i], sizes[i]);

        offsets[i] = writeOffset;
        writeOffset += sizes[i];
    }
    sizes.erase(sizes.begin() + regionID);
    offsets.erase(offsets.begin() + regionID);
    alignments.erase(alignments.begin() + regionID);

    //Korekta offsetów 
    for (uint32_t i = regionID; i < offsets.size(); ++i) {
        if (i == 0){
            continue;
        } 
        offsets[i] = offsets[i - 1] + sizes[i - 1] + GetPadding(i, offsets[i - 1] + sizes[i - 1]);
    }
}

void RenderBuffer::Reserve(uint32_t capacity) {
    if (capacity <= backBuffer.GetSize()) return;
    bufferChangeFlag.Set(BufferChange::Size);

    vg::Buffer newBuffer(capacity, bufferUsage);
    vg::Allocate(newBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    memcpy(newBuffer.MapMemory(), backBuffer.MapMemory(), backBuffer.GetSize());

    std::swap(backBuffer, newBuffer);
}

void RenderBuffer::Erase(uint32_t regionID, uint32_t eraseSize, uint32_t eraseOffset)   // TODO: spytac sie Slawka jak ma sie zachowywac
// NOTE przy skrajnych nwm czy bedzie sie dobrze zachowywać, czy ma terminować czy co robić idk
{
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Erase()");
    assert(eraseOffset + eraseSize <= sizes[regionID] && 
           "eraseOffset + eraseSize exceeds region size in RenderBuffer::Erase()");
    assert(eraseSize > 0 && "eraseSize must be greater than 0 in RenderBuffer::Erase()"); 
    bufferChangeFlag.Set(BufferChange::Contents); // do czego to jest nwm 

    uint32_t regionOffset = offsets[regionID];
    uint32_t regionSize = sizes[regionID];
    
    if (eraseOffset + eraseSize < regionSize) {
        uint32_t sourceOffset = regionOffset + eraseOffset + eraseSize;
        uint32_t destOffset = regionOffset + eraseOffset;
        uint32_t moveSize = regionSize - (eraseOffset + eraseSize);
        
        memmove(backBuffer.MapMemory() + destOffset, 
                backBuffer.MapMemory() + sourceOffset, 
                moveSize);
    }

    sizes[regionID] -= eraseSize;
    uint32_t currentOffset = offsets[regionID] + sizes[regionID];

    for (uint32_t i = regionID + 1; i < offsets.size(); i++) {
        uint32_t padding = GetPadding(i, currentOffset);
        currentOffset += padding;
        
        memmove(backBuffer.MapMemory() + currentOffset,
                backBuffer.MapMemory() + offsets[i],
                sizes[i]);
        
        // Aktualizuj offset regionu
        offsets[i] = currentOffset;
        currentOffset += sizes[i];
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
    memcpy(backBuffer.MapMemory() + offsets[regionID] + writeOffset, data, dataSize);
}

void RenderBuffer::Write(uint32_t regionID, const void *data) { Write(regionID, data, sizes[regionID]); }

uint32_t RenderBuffer::Read(uint32_t regionID, void *data, uint32_t maximumReadSize, uint32_t readOffset) {
    assert(regionID < sizes.size() && "Invalid regionID");
    assert(readOffset < sizes[regionID] && "Read offset larger than region size");

    maximumReadSize = std::min(maximumReadSize, sizes[regionID] - readOffset);

    if (data) memcpy(data, backBuffer.MapMemory() + offsets[regionID] + readOffset, maximumReadSize);

    return maximumReadSize;
}

int RenderBuffer::GetCapacity() const { return backBuffer.GetSize(); }

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
