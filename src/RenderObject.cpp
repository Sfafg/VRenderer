#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include "DebugRendering.h"

RenderObject::RenderObject(
    Mesh *mesh, Material *material, uint32_t objectByteSize, const void *data, bool debugObject
) {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    BatchArray::batchArray->AddObject(this, mesh, material, objectByteSize);
    if (data) SetData(data, objectByteSize);
}

RenderObject::RenderObject() : batchIndex(-1U), objectDataIndex(0) {}

RenderObject::RenderObject(RenderObject &&o) : RenderObject() {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);
    if (batchIndex != -1U) BatchArray::batchArray->renderObjects[batchIndex][objectDataIndex] = this;
}

RenderObject &RenderObject::operator=(RenderObject &&o) {
    if (this == &o) return *this;

    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    if (batchIndex != -1U && o.batchIndex != -1U) {
        std::swap(
            BatchArray::batchArray->renderObjects[batchIndex][objectDataIndex],
            BatchArray::batchArray->renderObjects[o.batchIndex][o.objectDataIndex]
        );
    } else if (o.batchIndex != -1U) BatchArray::batchArray->renderObjects[o.batchIndex][o.objectDataIndex] = this;

    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);

    return *this;
}

RenderObject::~RenderObject() {
    if (batchIndex == -1U) return;
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    BatchArray::batchArray->RemoveObject(this);
    batchIndex = -1U;
}

void RenderObject::SetData(const void *data, uint32_t byteSize) {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    BatchArray::batchArray->objectBuffer.Write(
        batchIndex, data, byteSize, BatchArray::batchArray->objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}

void RenderObject::ReadData(void *data) {
    assert(BatchArray::batchArray && "Current batchArray needs to be assigned!");
    BatchArray::batchArray->objectBuffer.Read(
        batchIndex, data, BatchArray::batchArray->objectBuffer.Alignment(batchIndex),
        BatchArray::batchArray->objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}

void RenderObject::Reserve(class Mesh *mesh, class Material *material, uint objectCount, uint objectSize) {
    if (!BatchArray::Exists(mesh, material)) BatchArray::Add(mesh, material, objectSize);
    uint id = BatchArray::Get(mesh, material);
    BatchArray::ReserveObjects(id, objectCount);
}

void RenderObject::ShrinkToFit(class Mesh *mesh, class Material *material) {
    if (!BatchArray::Exists(mesh, material)) return;
    uint id = BatchArray::Get(mesh, material);
    BatchArray::ShrinkToFit(id);
}

void RenderObject::SetLOD(
    class Mesh *mesh, class Material *material, uint objectSize,
    const std::vector<std::tuple<class Mesh *, class Material *>> &lods
) {
    if (!BatchArray::Exists(mesh, material)) BatchArray::Add(mesh, material, objectSize);
    uint id = BatchArray::Get(mesh, material);
    BatchArray::SetLOD(id, lods);
}
