#pragma once
#include "Mesh.h"
#include "Material.h"
#include "RenderBuffer.h"
#include <algorithm>

struct Batch {
    Material *material;
    Mesh *mesh;
    uint32_t instanceCount = 0;

    bool operator==(const Batch &o) const {
        return material->index == o.material->index && material->variant == o.material->variant &&
               mesh->index == o.mesh->index;
    }
    bool operator<(const Batch &o) const {
        return material->index < o.material->index || material->variant < o.material->variant ||
               mesh->index < o.mesh->index;
    }
};

class RenderObject {
    friend class Renderer;

    static std::vector<Batch> batches;
    static std::vector<RenderBuffer> batchBuffers;
    static std::vector<RenderObject *> renderObjects;
    uint32_t batchIndex;
    uint32_t batchDataIndex;

  public:
    RenderObject(Mesh *mesh, Material *material, uint32_t batchDataByteSize = 0, const void *data = nullptr);
    template <typename T> RenderObject(Mesh *mesh, Material *material, const T &batchData);

    RenderObject();
    RenderObject(RenderObject &&);
    RenderObject &operator=(RenderObject &&);
    RenderObject(const RenderObject &) = delete;
    RenderObject &operator=(const RenderObject &) = delete;
    ~RenderObject();

    void SetBatchData(const void *data, uint32_t byteSize);
    template <typename T> void SetBatchData(const T &data);

    void ReadBatchData(void *data);
    template <typename T> T ReadBatchData();
};

template <typename T>
RenderObject::RenderObject(Mesh *mesh, Material *material, const T &batchData)
    : RenderObject(mesh, material, sizeof(T), &batchData) {}

template <typename T> void RenderObject::SetBatchData(const T &data) { return SetBatchData(&data, sizeof(T)); }

template <typename T> T RenderObject::ReadBatchData() {
    T t;
    ReadBatchData(&t);
    return t;
}
