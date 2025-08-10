#pragma once
#include "VG/VG.h"
#include <array>

// Allow for inserting and removing mesh data
// Allow growing and shrinking mesh data
// Implement mesh usage like Static, Dynamic, DynamicLength
class Mesh {

    struct MeshMetaData {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t vertexOffset;
    };

    static vg::Buffer combinedBuffer;
    static vg::Buffer meshDataBuffer;
    static uint64_t vertexBufferSize;

    uint32_t index;

    friend class Renderer;

  public:
    Mesh(int vertexCount, int vertexByteSize, void *vertexData, int indexCount, int indexByteSize, void *indexData);

    template <typename T, typename TIndex>
    Mesh(int vertexCount, T *vertices, int indexCount, TIndex *indices)
        : Mesh(vertexCount, sizeof(T), vertices, indexCount, sizeof(TIndex), indices) {}

    Mesh();
    Mesh(Mesh &&) = default;
    Mesh &operator=(Mesh &&) = default;
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    ~Mesh();

    MeshMetaData GetMeshMetaData() const;
};
