#pragma once
#include "VG/VG.h"
#include <vector>

/**
 * @brief Enumerator specyfing how frequently data is updated, it is used when deciding what data to copy from swap to
 * swap.
 */
enum class NextUpdate {
    None = 0,
    NextFrame,
};

class RenderBuffer {
  private:
    std::vector<uint32_t> sizes;
    std::vector<uint32_t> alignments;
    std::vector<uint32_t> offsets;
    std::unordered_map<uint32_t, uint32_t> idToIndex;
    uint32_t idCounter;

    uint32_t size;
    vg::Flags<vg::BufferUsage> bufferUsage;
    vg::Buffer frontBuffer; // Rendered to.
    vg::Buffer backBuffer;  // Written to.

    void Swap();

  public:
    using Region = uint32_t;

    RenderBuffer(int capacity, vg::Flags<vg::BufferUsage> bufferUsage);

    operator vg::Buffer &();
    operator const vg::Buffer &() const;

    Region Allocate(int byteSize, int alignment);
    void Reallocate(Region &region, int newByteSize);
    void Deallocate(Region &&region);
    void Reserve(int capacity);

    void Write(const Region &region, const void *data, uint32_t dataSize, uint32_t writeOffset = 0);
    void Write(const Region &region, const void *data);
    template <typename T> void Write(const Region &region, const T &data, uint32_t writeOffset = 0);

    int GetCapacity() const;
    int GetSize() const;

    uint32_t Size(const Region &region) const;
    uint32_t Alignment(const Region &region) const;
    uint32_t Offset(const Region &region) const;
    uint32_t GetPadding(const Region &region, uint32_t offset) const;
    void MoveRegionVariable(Region &&from, Region &&to);

    friend class Renderer;
};

inline void RenderBuffer::Swap() {
    std::swap(frontBuffer, backBuffer);
    if (frontBuffer.GetSize() != backBuffer.GetSize()) {
        backBuffer = vg::Buffer(frontBuffer.GetSize(), bufferUsage);
        vg::Allocate(backBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    }
    memcpy(backBuffer.GetMemory(), frontBuffer.GetMemory(), frontBuffer.GetSize());
}

inline RenderBuffer::RenderBuffer(int capacity, vg::Flags<vg::BufferUsage> bufferUsage)
    : frontBuffer(capacity, bufferUsage), backBuffer(capacity, bufferUsage), size(0), bufferUsage(bufferUsage),
      idCounter(0) {
    vg::Allocate(frontBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
    vg::Allocate(backBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
}

inline RenderBuffer::operator vg::Buffer &() { return frontBuffer; }

inline RenderBuffer::operator const vg::Buffer &() const { return frontBuffer; }

inline RenderBuffer::Region RenderBuffer::Allocate(int byteSize, int alignment) {
    int currentSize = this->size;
    int padding = (alignment - currentSize % alignment) % alignment;

    size += padding + byteSize;
    if (size >= backBuffer.GetSize()) Reserve(size);

    Region region = idCounter++;
    sizes.push_back(byteSize);
    alignments.push_back(alignment);
    offsets.push_back(currentSize + padding);
    idToIndex[idCounter] = offsets.size();

    return region;
}

inline void RenderBuffer::Reallocate(RenderBuffer::Region &region, int newByteSize) {
    // TO DO: Add case for shrinking.

    uint32_t regionIndex = idToIndex[region];

    if (regionIndex == offsets.size()) {
        Deallocate(std::move(region));

        Region missingRegion = Allocate(newByteSize, Alignment(region));
        region = missingRegion;
        return;
    }

    Region newRegion = Allocate(newByteSize, Alignment(region));
    memcpy(backBuffer.MapMemory() + Offset(newRegion), backBuffer.MapMemory() + Offset(region), Size(region));

    uint32_t offset = offsets[regionIndex];
    if (region != 0) offset = offsets[regionIndex - 1] + sizes[regionIndex - 1];

    for (auto i = regions.begin() + region.index + 1; i != regions.end(); i++) {
        Region &reg = **i;
        auto padding = GetPadding(reg, offset);
        offset += padding;

        memcpy(backBuffer.MapMemory() + offset, backBuffer.MapMemory() + Offset(reg), Size(region));

        offset += Size(reg);
    }
    size = offset;

    std::swap(region, newRegion);
}

inline void RenderBuffer::Deallocate(RenderBuffer::Region &&region) {
    uint32_t offset = Offset(region);
    if (region.index != 0) offset = Offset(*regions[region.index - 1]) + Size(*regions[region.index - 1]);

    for (auto i = regions.begin() + region.index + 1; i != regions.end(); i++) {
        Region &reg = **i;
        auto padding = GetPadding(reg, offset);
        offset += padding;

        memcpy(backBuffer.MapMemory() + offset, backBuffer.MapMemory() + Offset(reg), Size(region));

        offset += Size(reg);
    }
    size = offset;
}

inline void RenderBuffer::Reserve(int capacity) {
    if (capacity <= backBuffer.GetSize()) return;

    vg::Buffer newBuffer(capacity, bufferUsage);
    vg::Allocate(newBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    memcpy(newBuffer.MapMemory(), backBuffer.MapMemory(), backBuffer.GetSize());

    std::swap(backBuffer, newBuffer);
}

inline void RenderBuffer::Write(const Region &region, const void *data, uint32_t dataSize, uint32_t writeOffset) {
    memcpy(backBuffer.GetMemory() + Offset(region) + writeOffset, data, dataSize);
}
inline void RenderBuffer::Write(const Region &region, const void *data) { Write(region, data, Size(region)); }

template <typename T> inline void RenderBuffer::Write(const Region &region, const T &data, uint32_t writeOffset) {
    Write(region, &data, sizeof(T), writeOffset);
}

inline int RenderBuffer::GetCapacity() const { return backBuffer.GetSize(); }

inline int RenderBuffer::GetSize() const { return size; }

inline RenderBuffer::Region::Region(uint32_t index) : index(index) {}

inline RenderBuffer::Region::Region() : index(-1) {}

inline RenderBuffer::Region::operator uint32_t() const { return index; }

inline uint32_t RenderBuffer::Size(const Region &region) const { return sizes[region.index]; }

inline uint32_t RenderBuffer::Alignment(const Region &region) const { return alignments[region.index]; }

inline uint32_t RenderBuffer::Offset(const Region &region) const { return offsets[region.index]; }

inline uint32_t RenderBuffer::GetPadding(const Region &region, uint32_t offset) const {
    return (alignments[region.index] - offset % alignments[region.index]) % alignments[region.index];
}
inline void RenderBuffer::MoveRegionVariable(Region &&from, Region &&to) {
    std::swap(from.index, to.index);
    if (to.index != -1) regions[to.index] = &to;
}
