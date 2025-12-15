#pragma once
#include "RenderBuffer.h"
#include <vector>
#include <glm/glm.hpp>

class MeshManager {
  public:
    MeshManager(int maxFramesInFlight);

    MeshManager();
    MeshManager(MeshManager &&);
    MeshManager &operator=(MeshManager &&);
    MeshManager(const MeshManager &) = delete;
    MeshManager &operator=(const MeshManager &) = delete;
    ~MeshManager();

  private:
    friend class Renderer;
    friend class Mesh;
    RenderBuffer vertexBuffer;
    RenderBuffer indexBuffer;
    RenderBuffer meshDataBuffer;
    std::vector<class Mesh *> meshes;
};

class Mesh {
    friend class Renderer;
    friend class RenderObject;
    friend class BatchManager;
    friend class GPURenderSystem;

    struct MeshMetaData {
        glm::vec3 boundsMin;
        float padding1;
        glm::vec3 boundsMax;
        float padding2;
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t vertexOffset;
        float padding3;
    };

    uint32_t index;

  public:
    Mesh(
        glm::vec3 boundsMin, glm::vec3 boundsMax, int vertexCount, int vertexByteSize, void *vertexData, int indexCount,
        int indexByteSize, void *indexData
    );

    template <typename T, typename TIndex>
    Mesh(glm::vec3 boundsMin, glm::vec3 boundsMax, int vertexCount, T *vertices, int indexCount, TIndex *indices)
        : Mesh(boundsMin, boundsMax, vertexCount, sizeof(T), vertices, indexCount, sizeof(TIndex), indices) {}

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
