#pragma once
#include "RenderBuffer.h"

class Material;
class Mesh;
class RenderObject;

class Batch {
  private:
    Batch();
    uint LodCount() const;
    uint LodPointer() const;
    uint ParentPointer() const;
    void SetLodCount(uint count);
    void SetLodPointer(uint pointer);
    void SetParentPointer(uint pointer);
    Batch *GetNextLod() const;
    Batch *GetLodParent() const;
    static const uint NULL_LOD = 0b1111111111111111111111111111;

  public:
    struct InstanceMapping {
        uint objectDataIndex;
        uint batchIndex;
    };
    static uint totalObjects;

    static std::vector<Batch> batches;
    static std::vector<std::tuple<uint16_t, uint16_t>> materialIndices;
    static std::vector<uint> instanceMappingRegionIndex;
    static std::vector<std::vector<RenderObject *>> renderObjects;
    static RenderBuffer instanceMappingBuffer;
    static RenderBuffer drawCallBuffer;
    static RenderBuffer objectBuffer;

    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex = -1U;
    uint meshIndex = -1U;
    uint batchDataElementSize;
    uint lodCountPointerParent;
    uint firstObject;

    static std::tuple<uint, bool> Add(Material *material, Mesh *mesh, uint objectByteSize, bool isLod = false);
    static std::tuple<uint, bool> Get(Material *material, Mesh *mesh);
    static void Remove(uint index);
    static void ReserveObjects(uint batchIndex, uint objectCount);
    static void AddLOD(uint batchIndex, Material *material, Mesh *mesh);

    bool operator==(const std::tuple<Material *, Mesh *> &o) const;
    bool operator<(const std::tuple<Material *, Mesh *> &o) const;

  private:
    friend Mesh;
    friend Material;
    friend RenderObject;

    static void NotifyMaterialDestroy(uint index);
    static void NotifyVariantDestroy(uint materialIndex, uint index);
    static void NotifyMeshDestroy(uint index);

    static void FixAfterObjectChange(uint batchID, uint firstObject, int objectDelta);
    static void FixAfterBatchChange(uint firstBatch, int batchDelta);

    static void AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize);
    static void RemoveObject(RenderObject *renderObject);
};
