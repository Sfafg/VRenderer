#pragma once
#include "VG/VG.h"
#include <cassert>
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

    uint32_t size;
    vg::Flags<vg::BufferUsage> bufferUsage;
    vg::Buffer frontBuffer; // Rendered to.
    vg::Buffer backBuffer;  // Written to.

    void Swap();

  public:
    RenderBuffer();
    RenderBuffer(int capacity, vg::Flags<vg::BufferUsage> bufferUsage);
    RenderBuffer(RenderBuffer &&) = default;
    RenderBuffer &operator=(RenderBuffer &&) = default;
    RenderBuffer(const RenderBuffer &) = delete;
    RenderBuffer &operator=(const RenderBuffer &) = delete;
    ~RenderBuffer();

    operator vg::Buffer &();
    operator const vg::Buffer &() const;

    uint32_t Allocate(int byteSize, int alignment);
    uint32_t Reallocate(uint32_t regionID, int newByteSize);
    void Deallocate(uint32_t regionID);
    void Reserve(int capacity);

    void Write(uint32_t regionID, const void *data, uint32_t dataSize, uint32_t writeOffset = 0);
    void Write(uint32_t regionID, const void *data);
    template <typename T> void Write(uint32_t regionID, const T &data, uint32_t writeOffset = 0);

    int GetCapacity() const;
    int GetSize() const;

    uint32_t Size(uint32_t regionID) const;
    uint32_t Alignment(uint32_t regionID) const;
    uint32_t Offset(uint32_t regionID) const;
    uint32_t GetPadding(uint32_t regionID, uint32_t previousEnd) const;

    friend class Renderer;
    friend class Material;
    
};

inline void RenderBuffer::Swap() {
    std::swap(frontBuffer, backBuffer);
    if (frontBuffer.GetSize() != backBuffer.GetSize()) {
        backBuffer = vg::Buffer(frontBuffer.GetSize(), bufferUsage);
        vg::Allocate(backBuffer, {vg::MemoryProperty::HostCoherent, vg::MemoryProperty::HostVisible});
    }
    memcpy(backBuffer.GetMemory(), frontBuffer.GetMemory(), frontBuffer.GetSize());
}

inline RenderBuffer::RenderBuffer() {}

inline RenderBuffer::RenderBuffer(int capacity, vg::Flags<vg::BufferUsage> bufferUsage)
    : frontBuffer(capacity, bufferUsage), backBuffer(capacity, bufferUsage), size(0), bufferUsage(bufferUsage) {
    vg::Allocate(frontBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
    vg::Allocate(backBuffer, {vg::MemoryProperty::HostVisible, vg::MemoryProperty::HostCoherent});
}

inline RenderBuffer::~RenderBuffer() {}

inline RenderBuffer::operator vg::Buffer &() { return frontBuffer; }

inline RenderBuffer::operator const vg::Buffer &() const { return frontBuffer; }

inline uint32_t RenderBuffer::Allocate(int byteSize, int alignment) {
    int currentSize = this->size;
    int padding = (alignment - currentSize % alignment) % alignment;

    size += padding + byteSize;
    if (size >= backBuffer.GetSize()) Reserve(size);

    sizes.push_back(byteSize);
    alignments.push_back(alignment);
    offsets.push_back(currentSize + padding);

    return sizes.size() - 1;
}

inline uint32_t RenderBuffer::Reallocate(uint32_t regionID, int newByteSize) {
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

inline void RenderBuffer::Deallocate(uint32_t regionID) {
    uint32_t offset = offsets[regionID];
    if (regionID != 0) offset = offsets[regionID - 1 + sizes[regionID - 1]];

    for (auto i = regionID + 1; i < offsets.size(); i++) {
        auto padding = (alignments[i] - offset % alignments[i]) % alignments[i];
        offset += padding;

        memcpy(backBuffer.MapMemory() + offset, backBuffer.MapMemory() + offsets[i], sizes[i]);

        offsets[i] = offset;
        offset += sizes[i];
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

inline void RenderBuffer::Write(uint32_t regionID, const void *data, uint32_t dataSize, uint32_t writeOffset) {
    memcpy(backBuffer.GetMemory() + offsets[regionID] + writeOffset, data, dataSize);
}
inline void RenderBuffer::Write(uint32_t regionID, const void *data) { Write(regionID, data, sizes[regionID]); }

template <typename T> inline void RenderBuffer::Write(uint32_t regionID, const T &data, uint32_t writeOffset) {
    Write(regionID, &data, sizeof(T), writeOffset);
}

inline int RenderBuffer::GetCapacity() const { return backBuffer.GetSize(); }

inline int RenderBuffer::GetSize() const { return size; }

inline uint32_t RenderBuffer::Size(uint32_t regionID) const {
    return (regionID < sizes.size()) ? sizes[regionID] : 0;
}

inline uint32_t RenderBuffer::Alignment(uint32_t regionID) const {
    return (regionID < alignments.size()) ? alignments[regionID] : 0;
}

inline uint32_t RenderBuffer::Offset(uint32_t regionID) const {
    return (regionID < offsets.size()) ? offsets[regionID] : 0;
}

inline uint32_t RenderBuffer::GetPadding(uint32_t regionID, uint32_t previousEnd) const {
    if (regionID >= alignments.size()) return 0;
    return (alignments[regionID] - previousEnd % alignments[regionID]) % alignments[regionID];
}
