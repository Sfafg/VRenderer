#pragma once
#include "RenderBuffer.h"

struct Batch {
    class Material *material;
    class Mesh *mesh;
    RenderBuffer batchBuffer;
    std::vector<class RenderObject *> renderObjects;

    Batch(class Material *material, class Mesh *mesh);

    Batch();
    Batch(Batch &&);
    Batch &operator=(Batch &&);
    Batch(const Batch &) = delete;
    Batch &operator=(const Batch &) = delete;
    ~Batch();

    bool operator==(const Batch &o) const;
    bool operator<(const Batch &o) const;
};

class RenderObject {
    friend class Renderer;

    static std::vector<Batch> batches;
    uint32_t batchIndex;
    uint32_t batchDataIndex;

  public:
    RenderObject(
        class Mesh *mesh, class Material *material, uint32_t batchDataByteSize = 0, const void *data = nullptr
    );
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
