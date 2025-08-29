#pragma once
#include "RenderBuffer.h"
struct Batch {
    struct BatchMetaData {
        uint firstInstace;
        uint batchDataElementSize;
        uint materialIndex;
        uint meshIndex;
    };
    static RenderBuffer batchMetaData;
    static RenderBuffer drawCallBuffer;

    class Material *material;
    class Mesh *mesh;
    std::vector<class RenderObject *> renderObjects;

    Batch(class Material *material, class Mesh *mesh, uint32_t batchDataElementSize);

    Batch();
    Batch(Batch &&);
    Batch &operator=(Batch &&);
    Batch(const Batch &) = delete;
    Batch &operator=(const Batch &) = delete;
    ~Batch();

    bool operator==(const std::tuple<class Material *, class Mesh *> &o) const;
    bool operator<(const std::tuple<class Material *, class Mesh *> &o) const;
};
