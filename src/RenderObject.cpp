#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Buffer.h"

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t objectByteSize, const void *data) {
    Batch::AddObject(this, mesh, material, objectByteSize);
    if (data) SetData(data, objectByteSize);
}

RenderObject::RenderObject() : batchIndex(-1U), objectDataIndex(0) {}

RenderObject::RenderObject(RenderObject &&o) {
    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);
    if (batchIndex != -1U) Batch::renderObjects[batchIndex][objectDataIndex] = this;
}

RenderObject &RenderObject::operator=(RenderObject &&o) {
    if (this == &o) return *this;

    if (batchIndex != -1U && o.batchIndex != -1U) {
        std::swap(
            Batch::renderObjects[batchIndex][objectDataIndex], Batch::renderObjects[o.batchIndex][o.objectDataIndex]
        );
    } else if (o.batchIndex != -1U) Batch::renderObjects[o.batchIndex][o.objectDataIndex] = this;

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
    Batch::objectBuffer.Write(batchIndex, data, byteSize, Batch::objectBuffer.Alignment(batchIndex) * objectDataIndex);
}

void RenderObject::ReadData(void *data) {
    Batch::objectBuffer.Read(
        batchIndex, data, Batch::objectBuffer.Alignment(batchIndex),
        Batch::objectBuffer.Alignment(batchIndex) * objectDataIndex
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
