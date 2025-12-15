#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include "DebugRendering.h"

RenderObject::RenderObject(
    Mesh *mesh, Material *material, uint32_t objectByteSize, const void *data, bool debugObject
) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    batchManager.AddObject(this, mesh, material, objectByteSize);
    if (data) SetData(data, objectByteSize);
}

RenderObject::RenderObject() : batchIndex(-1U), objectDataIndex(0) {}

RenderObject::RenderObject(RenderObject &&o) : RenderObject() {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);
    if (batchIndex != -1U) batchManager.renderObjects[batchIndex][objectDataIndex] = this;
}

RenderObject &RenderObject::operator=(RenderObject &&o) {
    if (this == &o) return *this;

    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    if (batchIndex != -1U && o.batchIndex != -1U) {
        std::swap(
            batchManager.renderObjects[batchIndex][objectDataIndex],
            batchManager.renderObjects[o.batchIndex][o.objectDataIndex]
        );
    } else if (o.batchIndex != -1U) batchManager.renderObjects[o.batchIndex][o.objectDataIndex] = this;

    std::swap(batchIndex, o.batchIndex);
    std::swap(objectDataIndex, o.objectDataIndex);

    return *this;
}

RenderObject::~RenderObject() {
    if (batchIndex == -1U) return;
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    batchManager.RemoveObject(this);
    batchIndex = -1U;
}

void RenderObject::SetData(const void *data, uint32_t byteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    batchManager.objectBuffer.Write(
        batchIndex, data, byteSize, batchManager.objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}

void RenderObject::ReadData(void *data) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    batchManager.objectBuffer.Read(
        batchIndex, data, batchManager.objectBuffer.Alignment(batchIndex),
        batchManager.objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}

void RenderObject::Reserve(class Mesh *mesh, class Material *material, uint objectCount, uint objectSize) {
    if (!BatchManager::Exists(mesh, material)) BatchManager::Add(mesh, material, objectSize);
    uint id = BatchManager::Get(mesh, material);
    BatchManager::ReserveObjects(id, objectCount);
}

void RenderObject::ShrinkToFit(class Mesh *mesh, class Material *material) {
    if (!BatchManager::Exists(mesh, material)) return;
    uint id = BatchManager::Get(mesh, material);
    BatchManager::ShrinkToFit(id);
}

void RenderObject::SetLOD(
    class Mesh *mesh, class Material *material, uint objectSize,
    const std::vector<std::tuple<class Mesh *, class Material *>> &lods
) {
    if (!BatchManager::Exists(mesh, material)) BatchManager::Add(mesh, material, objectSize);
    uint id = BatchManager::Get(mesh, material);
    BatchManager::SetLOD(id, lods);
}
