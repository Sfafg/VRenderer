#pragma once
#include "RenderBuffer.h"
#include "Batch.h"
#include "ByteView.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class RenderObject {
    friend class Renderer;
    friend class BatchArray;
    friend class GPURenderSystem;

    uint32_t batchIndex;
    uint32_t objectDataIndex;

  public:
    RenderObject(class Mesh *mesh, class Material *material, ByteView data, bool debugObject = false);

    RenderObject();
    RenderObject(RenderObject &&);
    RenderObject &operator=(RenderObject &&);
    RenderObject(const RenderObject &) = delete;
    RenderObject &operator=(const RenderObject &) = delete;
    ~RenderObject();

    void SetData(const void *data, uint32_t byteSize);
    template <typename T> void SetData(const T &data);

    void ReadData(void *data);
    template <typename T> T ReadData();

    void SetLOD(const std::vector<std::tuple<class Mesh *, class Material *>> &lods);

    static void Reserve(class Mesh *mesh, class Material *material, uint objectCount, uint objectSize);
    static void ShrinkToFit(class Mesh *mesh, class Material *material);
    static void SetLOD(
        class Mesh *mesh, class Material *material, uint objectSize,
        const std::vector<std::tuple<class Mesh *, class Material *>> &lods
    );
};

template <typename T> void RenderObject::SetData(const T &data) { return SetData(&data, sizeof(T)); }

template <typename T> T RenderObject::ReadData() {
    T t;
    ReadData(&t);
    return t;
}
