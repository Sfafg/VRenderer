#pragma once
#include "RenderBuffer.h"
#include <vector>

class Mesh {
    friend class Renderer;
    friend class RenderObject;
    friend class Batch;

    struct MeshMetaData {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t vertexOffset;
    };

    static RenderBuffer vertexBuffer;
    static RenderBuffer indexBuffer;
    static RenderBuffer meshDataBuffer;
    static std::vector<Mesh *> meshes;

    uint32_t index;

  public:
    Mesh(int vertexCount, int vertexByteSize, void *vertexData, int indexCount, int indexByteSize, void *indexData);

    template <typename T, typename TIndex>
    Mesh(int vertexCount, T *vertices, int indexCount, TIndex *indices)
        : Mesh(vertexCount, sizeof(T), vertices, indexCount, sizeof(TIndex), indices) {}

    Mesh();
    Mesh(Mesh &&);
    Mesh &operator=(Mesh &&);
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    ~Mesh();

    MeshMetaData GetMeshMetaData() const;
    uint32_t GetVertexCount() const;
    uint32_t GetIndexCount() const;

    void AppendVertices(const void *vertexData, uint32_t byteSize);
    void AppendIndices(const void *indexData, uint32_t byteSize);
    void EraseVertices(uint32_t count);
    void EraseIndices(uint32_t count);
    void WriteVertexData(const void *vertexData, uint32_t byteSize, uint32_t byteOffset);
    void WriteIndexData(const void *indexData, uint32_t byteSize, uint32_t byteOffset);
};
