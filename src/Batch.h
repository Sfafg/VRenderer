#pragma once
#include "RenderBuffer.h"
#include "DrawCallArray.h"

class Material;
class Mesh;
class RenderObject;

class BatchArray {
    friend class DrawCallArray;
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
    static uint GetTotalInstanceCount();

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

  private:
    bool _Exists(Mesh *mesh, Material *material);
    uint _Add(Mesh *mesh, Material *material, uint objectByteSize);
    uint _Get(Mesh *mesh, Material *material) const;
    void _Remove(uint batchIndex);
    void _SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods);
    void _ReserveObjects(uint batchIndex, uint objectCount);
    void _ShrinkToFit(uint batchIndex);

    uint _GetObjectCapacity(uint batchIndex);
    uint _GetObjectCount(uint batchIndex);

  private:
    friend RenderObject;
    void AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize);
    void RemoveObject(RenderObject *renderObject);

    void NotifyDrawCallInsert(uint index);
    void NotifyDrawCallDestroy(uint index);

  public:
    DrawCallArray drawCallArray;

    std::vector<Batch> batches;
    uint totalObjects = 0;
    std::vector<std::vector<RenderObject *>> renderObjects;
    RenderBuffer batchBuffer;
    RenderBuffer objectBuffer;
};
