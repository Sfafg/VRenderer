#pragma once
#include "RenderBuffer.h"

class Material;
class Mesh;
class RenderObject;

class BatchArray {
    friend class GPURenderSystem;
    friend class Renderer;

  public:
    static BatchArray *batchArray;

    static bool Exists(Mesh *mesh, Material *material);
    static uint Add(Mesh *mesh, Material *material, uint objectByteSize);
    static uint Get(Mesh *mesh, Material *material);
    static void Remove(uint batchIndex);
    static void SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods);
    static void ReserveObjects(uint batchIndex, uint objectCount);
    static void ShrinkToFit(uint batchIndex);

    static uint GetObjectCapacity(uint batchIndex);
    static uint GetObjectCount(uint batchIndex);

    BatchArray(int maxFramesInFlight, uint transparencyBucketCount);

    BatchArray();
    BatchArray(BatchArray &&);
    BatchArray &operator=(BatchArray &&);
    BatchArray(const BatchArray &) = delete;
    BatchArray &operator=(const BatchArray &) = delete;
    ~BatchArray();

  private:
  public:
    struct Batch {
        uint objectDataOffset;
        uint firstObjectIndex;
        uint objectDataElementSize;
        uint drawCall;
        uint lods[4];
    };

    struct DrawCall {
        uint indexCount;
        uint instanceCount;
        uint firstIndex;
        uint vertexOffset;
        uint firstInstance;
        uint materialIndex;
        uint meshIndex;

        bool operator==(const std::tuple<Material *, Mesh *> &o) const;
        bool operator<(const std::tuple<Material *, Mesh *> &o) const;
    };

  private:
    bool _Exists(Mesh *mesh, Material *material);
    uint _Add(Mesh *mesh, Material *material, uint objectByteSize);
    uint _Get(Mesh *mesh, Material *material);
    void _Remove(uint batchIndex);
    void _SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods);
    void _ReserveObjects(uint batchIndex, uint objectCount);
    void _ShrinkToFit(uint batchIndex);

    uint AddTransparentBatch(Mesh *mesh, Material *material, uint objectByteSize);

    uint _GetObjectCapacity(uint batchIndex);
    uint _GetObjectCount(uint batchIndex);

  private:
    uint GetDrawCall(Mesh *mesh, Material *material);
    void InsertDrawCall(uint index, Mesh* mesh, Material* material);
    void DeleteDrawCall(uint id);

    // uint AddOrGetDrawCall(Mesh *mesh, Material *material);
    // uint AddOrGetTransparentDrawCall(Mesh *mesh, Material *material);

    friend RenderObject;
    void AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize);
    void RemoveObject(RenderObject *renderObject);

    friend Mesh;
    friend Material;
    void NotifyMaterialDestroy(uint index);
    void NotifyVariantDestroy(uint materialIndex, uint index);
    void NotifyMeshDestroy(uint index);

  private:
  public:
    uint transparencyBucketCount = 0;
    uint firstTransparentDrawCall = 0;
    uint transparentDrawCallsCount = 0;
    std::vector<DrawCall> drawCalls;
    std::vector<std::tuple<uint, uint>> drawCallMaterialIndices;
    RenderBuffer drawCallBuffer;
    RenderBuffer instanceMappingBuffer;

    std::vector<Batch> batches;
    uint totalObjects = 0;
    std::vector<std::vector<RenderObject *>> renderObjects;
    RenderBuffer batchBuffer;
    RenderBuffer objectBuffer;
};
