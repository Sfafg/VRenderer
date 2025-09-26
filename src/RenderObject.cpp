#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t objectByteSize, const void *data) {
    Batch::AddObject(this, mesh, material, objectByteSize);
    if (data) SetData(data, objectByteSize);
}

RenderObject::RenderObject() : batchIndex(-1U), objectDataIndex(0) {}

RenderObject::RenderObject(RenderObject &&o) : RenderObject() {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);
    if (batchIndex != -1U) renderer.renderObjects[batchIndex][objectDataIndex] = this;
}

RenderObject &RenderObject::operator=(RenderObject &&o) {
    if (this == &o) return *this;

    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    if (batchIndex != -1U && o.batchIndex != -1U) {
        std::swap(
            renderer.renderObjects[batchIndex][objectDataIndex], renderer.renderObjects[o.batchIndex][o.objectDataIndex]
        );
    } else if (o.batchIndex != -1U) renderer.renderObjects[o.batchIndex][o.objectDataIndex] = this;

    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);

    return *this;
}

RenderObject::~RenderObject() {
    if (batchIndex == -1U) return;
    Batch::RemoveObject(this);
    batchIndex = -1U;
}

void RenderObject::SetData(const void *data, uint32_t byteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.objectBuffer.Write(
        batchIndex, data, byteSize, renderer.objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}

void RenderObject::ReadData(void *data) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    renderer.objectBuffer.Read(
        batchIndex, data, renderer.objectBuffer.Alignment(batchIndex),
        renderer.objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}

void RenderObject::Reserve(class Mesh *mesh, class Material *material, uint objectCount, uint objectSize) {
    if (!Batch::Exists(mesh, material)) Batch::Add(mesh, material, objectSize);
    uint id = Batch::Get(mesh, material);
    Batch::ReserveObjects(id, objectCount);
}

void RenderObject::ShrinkToFit(class Mesh *mesh, class Material *material) {
    if (!Batch::Exists(mesh, material)) return;
    uint id = Batch::Get(mesh, material);
    Batch::ShrinkToFit(id);
}

void RenderObject::SetLOD(
    class Mesh *mesh, class Material *material, uint objectSize,
    const std::vector<std::tuple<class Mesh *, class Material *>> &lods
) {
    if (!Batch::Exists(mesh, material)) Batch::Add(mesh, material, objectSize);
    uint id = Batch::Get(mesh, material);
    Batch::SetLOD(id, lods);
}
