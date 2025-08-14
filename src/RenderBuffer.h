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

enum class BufferChange { None = 0, Contents, Size };

class RenderBuffer {
    // TODO: Umożliwić usuwanie dowolnej części regionu poprzez erase, które zmniejsza rozmiar regionu. ZROBIONE
    // (prawie?)
    // TODO: Automatyczne updatowanie descriptorów.
    // TODO: Reallocate nie zmienia indeksu regionu więc powinien zwracać void.

  private:
    std::vector<uint32_t> sizes;
    std::vector<uint32_t> alignments;
    std::vector<uint32_t> offsets;

    uint32_t size;
    vg::Flags<vg::BufferUsage> bufferUsage;
    vg::Buffer frontBuffer; // Rendered to.
    vg::Buffer backBuffer;  // Written to.
    vg::Flags<BufferChange> bufferChangeFlag;

    bool Swap();

  public:
    RenderBuffer();
    RenderBuffer(uint32_t capacity, vg::Flags<vg::BufferUsage> bufferUsage);
    RenderBuffer(RenderBuffer &&) = default;
    RenderBuffer &operator=(RenderBuffer &&) = default;
    RenderBuffer(const RenderBuffer &) = delete;
    RenderBuffer &operator=(const RenderBuffer &) = delete;
    ~RenderBuffer();

    operator vg::Buffer &();
    operator const vg::Buffer &() const;

    uint32_t Allocate(uint32_t byteSize, uint32_t alignment);
    uint32_t Reallocate(uint32_t regionID, uint32_t newByteSize);
    void Deallocate(uint32_t regionID);
    void Erase(uint32_t regionID, uint32_t eraseSize, uint32_t eraseOffset = 0);
    void Reserve(uint32_t capacity);

    void Write(uint32_t regionID, const void *data, uint32_t dataSize, uint32_t writeOffset = 0);
    void Write(uint32_t regionID, const void *data);
    template <typename T>
        requires(!std::is_pointer_v<T>)
    void Write(uint32_t regionID, const T &data, uint32_t writeOffset = 0);

    /**
     * @brief Read portion of data from region, the user is responsible for allocation,
     * if data is nullptr the function only returns viable read size.
     *
     * @param regionID
     * @param data pointer to allocated data or nullptr
     * @param maxReadSize maximum number of bytes to read
     * @param readOffset byte offset inside of region
     *
     * @return number of bytes actually available to read or read.
     */
    uint32_t Read(uint32_t regionID, void *data, uint32_t maxReadSize = -1, uint32_t readOffset = 0);
    template <typename T> T Read(uint32_t regionID, uint32_t readOffset = 0);

    int GetCapacity() const;
    int GetSize() const;
    vg::Flags<BufferChange> GetChange() const;

    uint32_t Size(uint32_t regionID) const;
    uint32_t Alignment(uint32_t regionID) const;
    uint32_t Offset(uint32_t regionID) const;
    uint32_t GetPadding(uint32_t regionID, uint32_t offset) const;

    friend class Renderer;
    friend class Material;
};

template <typename T>
    requires(!std::is_pointer_v<T>)
void RenderBuffer::Write(uint32_t regionID, const T &data, uint32_t writeOffset) {
    Write(regionID, &data, sizeof(T), writeOffset);
}

template <typename T> T RenderBuffer::Read(uint32_t regionID, uint32_t readOffset) {
    T result;
    Read(regionID, &result, sizeof(T), readOffset);
    return result;
}
