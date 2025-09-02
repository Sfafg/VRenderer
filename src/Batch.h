#pragma once
#include "RenderBuffer.h"

class Material;
class Mesh;
class RenderObject;

class Batch {
  private:
    Batch();

  public:
    static std::vector<Batch> batches;
    static std::vector<std::tuple<uint16_t, uint16_t>> materialIndices;
    static RenderBuffer drawCallBuffer;
    static RenderBuffer instanceMappingBuffer;

    static std::vector<std::vector<RenderObject *>> renderObjects;
    static RenderBuffer objectBuffer;

    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex = -1U;
    uint meshIndex = -1U;
    uint batchDataElementSize;

    static std::tuple<uint, bool> Add(Material *material, Mesh *mesh, uint objectByteSize);
    static std::tuple<uint, bool> Get(Material *material, Mesh *mesh);
    static void Remove(uint index);
    static void ReserveObjects(uint batchIndex, uint objectCount);

    bool operator==(const std::tuple<Material *, Mesh *> &o) const;
    bool operator<(const std::tuple<Material *, Mesh *> &o) const;

  private:
    friend Mesh;
    friend Material;
    friend RenderObject;

    static void NotifyMaterialDestroy(uint index);
    static void NotifyVariantDestroy(uint materialIndex, uint index);
    static void NotifyMeshDestroy(uint index);

    static void AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize);
    static void RemoveObject(RenderObject *renderObject);
};
