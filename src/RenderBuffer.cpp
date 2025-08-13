#include "RenderBuffer.h"

void RenderBuffer::Swap() {
    std::swap(frontBuffer, backBuffer);
    if (frontBuffer.GetSize() != backBuffer.GetSize()) {
        backBuffer = vg::Buffer(frontBuffer.GetSize(), bufferUsage);
        vg::Allocate(backBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    }
    memcpy(backBuffer.MapMemory(), frontBuffer.MapMemory(), frontBuffer.GetSize());
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

    // TO DO: Add case for shrinking.
    if (regionID == offsets.size()) {
        Deallocate(regionID);
        return Allocate(newByteSize, alignments[regionID]);
    }

    uint32_t newRegion = Allocate(newByteSize, alignments[regionID]);
    memcpy(backBuffer.MapMemory() + offsets[newRegion], backBuffer.MapMemory() + offsets[regionID], sizes[regionID]);

    uint32_t offset = offsets[regionID];
    if (regionID != 0) offset = offsets[regionID - 1] + sizes[regionID - 1];

    for (auto i = regionID + 1; i < offsets.size(); i++) {
        auto padding = (alignments[i] - offset % alignments[i]) % alignments[i];
        offset += padding;

        memcpy(backBuffer.MapMemory() + offset, backBuffer.MapMemory() + offsets[i], sizes[i]);

        offsets[i] = offset;
        offset += sizes[i];
    }
    size = offset;

    return newRegion;
}

void RenderBuffer::Deallocate(uint32_t regionID, uint32_t size, uint32_t offset) {
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Deallocate()");
    assert(size + offset <= sizes[regionID] && "size + offset larger than region size in RenderBuffer::Deallocate()");
    uint32_t writeOffset = offsets[regionID];
    if (regionID != 0) writeOffset = offsets[regionID - 1 + sizes[regionID - 1]];

    for (auto i = regionID + 1; i < offsets.size(); i++) {
        auto padding = GetPadding(i, writeOffset);
        writeOffset += padding;

        memcpy(backBuffer.MapMemory() + writeOffset, backBuffer.MapMemory() + offsets[i], sizes[i]);

        offsets[i] = writeOffset;
        writeOffset += sizes[i];
    }
    size = writeOffset;
}
// TODO/TO DO NIEDOKONCZONE IDE NA OBIAD B) nie rób z tego osobnej funkcji, dodaj do deallocate offset i rozmiar
// dealokacji, które defaultowo są 0 i -1, bo logika będzie prawie taka sama, zrobiłem deklaracje ale musisz skończyć
// implementacje. A jednak nie wiem czy tego by się nie przydało dać do reallocate, tylko że offset tam nie pasuje, więc
// jakas funkcja do kopiowania danych??? Nie wiem, trzeba się zastanowić.

void RenderBuffer::Reserve(uint32_t capacity) {
    if (capacity <= backBuffer.GetSize()) return;

    vg::Buffer newBuffer(capacity, bufferUsage);
    vg::Allocate(newBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    memcpy(newBuffer.MapMemory(), backBuffer.MapMemory(), backBuffer.GetSize());

    std::swap(backBuffer, newBuffer);
}

void RenderBuffer::Write(uint32_t regionID, const void *data, uint32_t dataSize, uint32_t writeOffset) {
    assert(regionID < sizes.size() && "Invalid regionID in RenderBuffer::Write()");
    assert(data && "Null data pointer in RenderBuffer::Write()");
    assert(
        dataSize + writeOffset <= sizes[regionID] &&
        "dataSize + writeOffset larger than region size in RenderBuffer::Write()"
    );
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
