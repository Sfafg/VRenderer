#pragma once
#include "RenderBuffer.h"

class Material;
class Mesh;
class RenderObject;

class Batch {
  public:
    struct InstanceMapping {
        uint objectDataIndex;
        uint batchIndex;
    };
    static bool Exists(Mesh *mesh, Material *material);
    static uint Add(Mesh *mesh, Material *material, uint objectByteSize);
    static uint Get(Mesh *mesh, Material *material);
    static void Remove(uint batchIndex);
    static void SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods);
    static void ReserveObjects(uint batchIndex, uint objectCount);
    static void ShrinkToFit(uint batchIndex);

    static uint GetObjectCapacity(uint batchIndex);
    static uint GetObjectCount(uint batchIndex);

    bool operator==(const std::tuple<Material *, Mesh *> &o) const;
    bool operator<(const std::tuple<Material *, Mesh *> &o) const;

  private:
    friend RenderObject;
    static void AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize);
    static void RemoveObject(RenderObject *renderObject);

    friend Mesh;
    friend Material;
    static void NotifyMaterialDestroy(uint index);
    static void NotifyVariantDestroy(uint materialIndex, uint index);
    static void NotifyMeshDestroy(uint index);

  private:
    Batch();
    uint LodCount() const;
    uint LodPointer() const;
    uint ParentPointer() const;
    void SetLodCount(uint count);
    void SetLodPointer(uint pointer);
    void SetParentPointer(uint pointer);
    bool IsLOD() const;
    Batch *GetNextLod() const;
    Batch *GetLodParent() const;

    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint materialIndex = -1U;
    uint meshIndex = -1U;
    uint objectDataElementSize;
    uint lodCountPointerParent;
    uint firstObject;
    uint objectCount;
};
