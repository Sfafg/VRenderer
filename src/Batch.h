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
    static const uint NULL_LOD = 0b1111111111111111111111111111;

  public:
    struct InstanceMapping {
        uint objectDataIndex;
        uint batchIndex;
    };
    static uint totalObjects;

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
    uint lodCountPointerParent;
    // struct {
    //     uint lodCount : 4 = 0;
    //     uint lodPointer : 14 = NULL_LOD;
    //     uint parentPointer : 14 = NULL_LOD;
    // };

    static std::tuple<uint, bool> Add(Material *material, Mesh *mesh, uint objectByteSize);
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

    static void AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize);
    static void RemoveObject(RenderObject *renderObject);
};
