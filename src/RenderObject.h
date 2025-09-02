#pragma once
#include "RenderBuffer.h"
#include "Batch.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class RenderObject {
    friend class Renderer;
    friend class Batch;
    friend class GPURenderSystem;

    uint32_t batchIndex;
    uint32_t objectDataIndex;

  public:
    RenderObject(class Mesh *mesh, class Material *material, uint32_t objectByteSize = 0, const void *data = nullptr);
    template <typename T> RenderObject(class Mesh *mesh, class Material *material, const T &batchData);

    RenderObject();
    RenderObject(RenderObject &&);
    RenderObject &operator=(RenderObject &&);
    RenderObject(const RenderObject &) = delete;
    RenderObject &operator=(const RenderObject &) = delete;
    ~RenderObject();

    Batch &GetBatch();
    const Batch &GetBatch() const;

    void SetBatchData(const void *data, uint32_t byteSize);
    template <typename T> void SetBatchData(const T &data);

    void ReadBatchData(void *data);
    template <typename T> T ReadBatchData();
};

template <typename T>
RenderObject::RenderObject(class Mesh *mesh, class Material *material, const T &batchData)
    : RenderObject(mesh, material, sizeof(T), &batchData) {}

template <typename T> void RenderObject::SetBatchData(const T &data) { return SetBatchData(&data, sizeof(T)); }

template <typename T> T RenderObject::ReadBatchData() {
    T t;
    ReadBatchData(&t);
    return t;
}
