#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include <algorithm>
using namespace vg;
bool BatchManager::Exists(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._Exists(mesh, material);
}

uint BatchManager::Add(Mesh *mesh, Material *material, uint objectByteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._Add(mesh, material, objectByteSize);
}

uint BatchManager::Get(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._Get(mesh, material);
}

void BatchManager::Remove(uint batchIndex) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._Remove(batchIndex);
}

void BatchManager::SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._SetLOD(batchIndex, lods);
}

void BatchManager::ReserveObjects(uint batchIndex, uint objectCount) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._ReserveObjects(batchIndex, objectCount);
}

void BatchManager::ShrinkToFit(uint batchIndex) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._ShrinkToFit(batchIndex);
}

uint BatchManager::GetObjectCapacity(uint batchIndex) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._GetObjectCapacity(batchIndex);
}

uint BatchManager::GetObjectCount(uint batchIndex) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;
    return batchManager._GetObjectCount(batchIndex);
}

BatchManager::BatchManager() {}

BatchManager::BatchManager(int maxFramesInFlight, uint transparencyBucketCount)
    : transparencyBucketCount(transparencyBucketCount) {
    drawCallBuffer = RenderBuffer(maxFramesInFlight, {BufferUsage::StorageBuffer, BufferUsage::IndirectBuffer}, 0);
    instanceMappingBuffer = RenderBuffer(maxFramesInFlight, {BufferUsage::StorageBuffer, BufferUsage::VertexBuffer}, 0);
    batchBuffer = RenderBuffer(maxFramesInFlight, BufferUsage::StorageBuffer, 0);
    objectBuffer = RenderBuffer(maxFramesInFlight, BufferUsage::StorageBuffer, 0);
}

BatchManager::BatchManager(BatchManager &&o) : BatchManager() {
    std::swap(transparencyBucketCount, o.transparencyBucketCount);
    std::swap(firstTransparentDrawCall, o.firstTransparentDrawCall);
    std::swap(transparentDrawCallsCount, o.transparentDrawCallsCount);
    std::swap(drawCalls, o.drawCalls);
    std::swap(drawCallMaterialIndices, o.drawCallMaterialIndices);
    std::swap(drawCallBuffer, o.drawCallBuffer);
    std::swap(instanceMappingBuffer, o.instanceMappingBuffer);
    std::swap(batches, o.batches);
    std::swap(totalObjects, o.totalObjects);
    std::swap(renderObjects, o.renderObjects);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(objectBuffer, o.objectBuffer);
}

BatchManager &BatchManager::operator=(BatchManager &&o) {
    if (this == &o) return *this;

    std::swap(transparencyBucketCount, o.transparencyBucketCount);
    std::swap(firstTransparentDrawCall, o.firstTransparentDrawCall);
    std::swap(transparentDrawCallsCount, o.transparentDrawCallsCount);
    std::swap(drawCalls, o.drawCalls);
    std::swap(drawCallMaterialIndices, o.drawCallMaterialIndices);
    std::swap(drawCallBuffer, o.drawCallBuffer);
    std::swap(instanceMappingBuffer, o.instanceMappingBuffer);
    std::swap(batches, o.batches);
    std::swap(totalObjects, o.totalObjects);
    std::swap(renderObjects, o.renderObjects);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(objectBuffer, o.objectBuffer);

    return *this;
}

BatchManager::~BatchManager() {}

bool BatchManager::_Exists(Mesh *mesh, Material *material) { return Get(mesh, material) != -1U; }

uint BatchManager::_Add(Mesh *mesh, Material *material, uint objectByteSize) {
    uint index = AddOrGetDrawCall(mesh, material);

    // Create Batch.
    batchBuffer.Allocate(sizeof(Batch), sizeof(Batch));
    objectBuffer.Allocate(0, objectByteSize);

    Batch batch;
    batch.objectDataOffset = objectBuffer.Offset(batches.size()) / objectBuffer.Alignment(batches.size());
    batch.firstObjectIndex = totalObjects;
    batch.objectDataElementSize = objectByteSize;
    batch.drawCall = index;
    for (int i = 0; i < std::size(batch.lods); i++) batch.lods[i] = -1U;
    batchBuffer.Write(batches.size(), batch);

    renderObjects.emplace_back(std::vector<RenderObject *>());
    batches.emplace_back(std::move(batch));

    return batches.size() - 1;
}

uint BatchManager::_Get(Mesh *mesh, Material *material) {
    std::tuple<Material *, Mesh *> key(material, mesh);
    for (int i = 0; i < batches.size(); i++)
        if (drawCalls[batches[i].drawCall] == key) return i;

    return -1U;
}

void BatchManager::_Remove(uint index) {
    assert(index < batches.size() && "Invalid BatchID.");
    auto &batch = batches[index];

    std::set<uint> drawCallsToDelete = {batch.drawCall};
    for (int i = 0; i < std::size(batch.lods); i++) {
        if (batch.lods[i] == -1U) break;
        drawCallsToDelete.insert(batch.lods[i]);
    }

    // If for any draw call there is someone referecing it, don't destroy it.
    for (auto &b : batches) {
        if (&b == &batch) continue;

        if (drawCallsToDelete.contains(b.drawCall)) drawCallsToDelete.erase(b.drawCall);
        for (auto &lod : b.lods) {
            if (lod == -1U) break;
            if (drawCallsToDelete.contains(lod)) drawCallsToDelete.erase(lod);
        }

        if (drawCallsToDelete.empty()) break;
    }

    for (auto &&drawCall : drawCallsToDelete) DeleteDrawCall(drawCall);

    // Delete objects.
    objectBuffer.Deallocate(index);

    int objectCount = renderObjects[index].size();
    for (int i = index + 1; i < batches.size(); i++) {
        for (int j = 0; j < renderObjects[i].size(); j++) renderObjects[i][j]->batchIndex--;

        batches[i].objectDataOffset = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        batches[i].firstObjectIndex -= objectCount;
        batchBuffer.Write(i, batches[i].objectDataOffset, offsetof(Batch, objectDataOffset));
        batchBuffer.Write(i, batches[i].firstObjectIndex, offsetof(Batch, firstObjectIndex));
    }

    totalObjects -= objectCount;
    renderObjects.erase(renderObjects.begin() + index);

    // Delete batch
    batchBuffer.Deallocate(index);
    batches.erase(batches.begin() + index);
}

void BatchManager::_SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods) {
    assert(batchIndex < batches.size() && "Invalid Batch ID.");
    assert(lods.size() < 4 && "LOD count has to be less than 4.");
    assert(batches[batchIndex].lods[0] == -1U && "LOD changing has to be implemented.");

    auto &batch = batches[batchIndex];
    for (int i = 0; i < lods.size(); i++) {
        auto &&[mesh, material] = lods[i];
        uint index = AddOrGetDrawCall(mesh, material);
        batch.lods[i] = index;
    }
}

void BatchManager::_ReserveObjects(uint index, uint objectCount) {
    assert(index < batches.size() && "Invalid Batch ID.");
    auto &batch = batches[index];

    // Reserve objects for batch.
    renderObjects[index].reserve(objectCount);
    objectBuffer.Reallocate(index, objectBuffer.Alignment(index) * objectCount);
    for (int i = index + 1; i < batches.size(); i++) {
        batches[i].objectDataOffset = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        batchBuffer.Write(i, batches[i].objectDataOffset, offsetof(Batch, objectDataOffset));
    }

    // Reserve objects for drawCalls.
    uint minDrawCallID = batch.drawCall;
    instanceMappingBuffer.Reallocate(batch.drawCall, sizeof(uint) * objectCount);
    for (auto &lod : batch.lods) {
        if (lod == -1U) break;
        instanceMappingBuffer.Reallocate(lod, sizeof(uint) * objectCount);
        minDrawCallID = std::min(minDrawCallID, lod);
    }

    for (int i = minDrawCallID + 1; i < drawCalls.size(); i++) {
        drawCalls[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(DrawCall, firstInstance));
    }
}

void BatchManager::_ShrinkToFit(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    uint objectCount = GetObjectCount(index);
    uint objectCapacity = GetObjectCapacity(index);
    if (objectCount == 0) {
        Remove(index);
        return;
    }
    if (objectCount >= objectCapacity) return;

    ReserveObjects(index, objectCount);
}

uint BatchManager::_GetObjectCapacity(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return instanceMappingBuffer.Size(batches[index].drawCall) /
           instanceMappingBuffer.Alignment(batches[index].drawCall);
}

uint BatchManager::_GetObjectCount(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return renderObjects[index].size();
}

bool BatchManager::DrawCall::operator==(const std::tuple<Material *, Mesh *> &o) const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;

    auto id = this - &batchManager.drawCalls[0];
    auto [index, variant] = batchManager.drawCallMaterialIndices[id];
    return index == std::get<Material *>(o)->index && variant == std::get<Material *>(o)->variant &&
           meshIndex == std::get<Mesh *>(o)->index;
}
bool BatchManager::DrawCall::operator<(const std::tuple<Material *, Mesh *> &o) const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &batchManager = *currentRenderer->managers.batchManager;

    auto id = this - &batchManager.drawCalls[0];
    auto [index, variant] = batchManager.drawCallMaterialIndices[id];
    if (index < std::get<Material *>(o)->index) return true;
    if (index > std::get<Material *>(o)->index) return false;

    if (variant < std::get<Material *>(o)->variant) return true;
    if (variant > std::get<Material *>(o)->variant) return false;
    return meshIndex < std::get<Mesh *>(o)->index;
}

bool BatchManager::DrawCallExists(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    std::tuple<Material *, Mesh *> key(material, mesh);
    auto it = std::lower_bound(drawCalls.begin(), drawCalls.end(), key, [](auto &a, auto &b) { return a < b; });
    bool exists = it != drawCalls.end() && key == *it;
    uint index = it - drawCalls.begin();
}

int BatchManager::GetDrawCall(Mesh *mesh, Material *material);
void BatchManager::InsertDrawCall(Mesh *mesh, Material *material);
void BatchManager::DeleteDrawCall(uint id);

uint BatchManager::AddOrGetDrawCall(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    std::tuple<Material *, Mesh *> key(material, mesh);
    auto it = std::lower_bound(drawCalls.begin(), drawCalls.end(), key, [](auto &a, auto &b) { return a < b; });
    bool exists = it != drawCalls.end() && key == *it;
    uint index = it - drawCalls.begin();

    if (!exists) {
        auto &materialBuffer = currentRenderer->managers.materialManager->materialBuffer;

        drawCallBuffer.Allocate(sizeof(DrawCall), sizeof(DrawCall), index);
        instanceMappingBuffer.Allocate(0, sizeof(uint), index);

        DrawCall drawCall;
        drawCall.firstInstance = instanceMappingBuffer.Offset(index) / instanceMappingBuffer.Alignment(index);
        drawCall.meshIndex = mesh->index;
        drawCall.materialIndex = 0;
        if (materialBuffer.Alignment(material->index) != 0)
            drawCall.materialIndex =
                materialBuffer.Offset(material->index) / materialBuffer.Alignment(material->index) + material->variant;
        drawCallBuffer.Write(index, drawCall);

        drawCalls.emplace(drawCalls.begin() + index, std::move(drawCall));
        drawCallMaterialIndices.insert(drawCallMaterialIndices.begin() + index, {material->index, material->variant});

        // Fix pointers.
        for (int i = 0; i < batches.size(); i++) {
            bool update = false;
            if (batches[i].drawCall == -1U) continue;
            if (batches[i].drawCall > index) {
                batches[i].drawCall++;
                update = true;
            }

            for (int j = 0; j < 4; j++) {
                if (batches[i].lods[j] == -1U) break;
                if (batches[i].lods[j] > index) {
                    batches[i].lods[j]++;
                    update = true;
                }
            }

            if (update) batchBuffer.Write(i, batches[i]);
        }

        // Fix first instance.
        for (int i = index + 1; i < drawCalls.size(); i++) {
            drawCalls[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
            drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(DrawCall, firstInstance));
        }
    }

    return index;
}

uint BatchManager::AddOrGetTransparentDrawCall(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    std::tuple<Material *, Mesh *> key(material, mesh);
    std::span<DrawCall> transparentDrawCalls = {
        drawCalls.begin() + firstTransparentDrawCall,
        drawCalls.begin() + firstTransparentDrawCall + transparentDrawCallsCount
    };

    auto it = std::lower_bound(transparentDrawCalls.begin(), transparentDrawCalls.end(), key, [](auto &a, auto &b) {
        return a < b;
    });
    bool exists = it != transparentDrawCalls.end() && key == *it;
    uint transparentIndex = it - transparentDrawCalls.begin();

    if (!exists) {
        auto &materialBuffer = currentRenderer->managers.materialManager->materialBuffer;

        drawCallBuffer.Reserve(drawCallBuffer.GetCapacity() + sizeof(DrawCall) * transparencyBucketCount);
        for (int i = 0; i < transparencyBucketCount; i++) {
            uint index = (i * transparentDrawCallsCount) + transparentIndex + firstTransparentDrawCall;
            drawCallBuffer.Allocate(sizeof(DrawCall), sizeof(DrawCall), index);
            instanceMappingBuffer.Allocate(0, sizeof(uint), index);

            DrawCall drawCall;
            drawCall.firstInstance = instanceMappingBuffer.Offset(index) / instanceMappingBuffer.Alignment(index);
            drawCall.meshIndex = mesh->index;
            drawCall.materialIndex = 0;
            if (materialBuffer.Alignment(material->index) != 0)
                drawCall.materialIndex =
                    materialBuffer.Offset(material->index) / materialBuffer.Alignment(material->index) +
                    material->variant;
            drawCallBuffer.Write(index, drawCall);

            drawCalls.emplace(drawCalls.begin() + index, std::move(drawCall));
            drawCallMaterialIndices.insert(
                drawCallMaterialIndices.begin() + index, {material->index, material->variant}
            );

            // Fix first instance.
            for (int i = index + 1; i < drawCalls.size(); i++) {
                drawCalls[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
                drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(DrawCall, firstInstance));
            }
        }

        uint index = transparentIndex + firstTransparentDrawCall;

        // Fix pointers.
        for (int i = 0; i < batches.size(); i++) {
            bool update = false;
            if (batches[i].drawCall == -1U) continue;
            if (batches[i].drawCall > index) {
                batches[i].drawCall++;
                update = true;
            }

            for (int j = 0; j < 4; j++) {
                if (batches[i].lods[j] == -1U) break;
                if (batches[i].lods[j] > index) {
                    batches[i].lods[j]++;
                    update = true;
                }
            }

            if (update) batchBuffer.Write(i, batches[i]);
        }
    }

    return transparentIndex + firstTransparentDrawCall;
}

void BatchManager::DeleteDrawCall(uint id) {
    drawCallBuffer.Deallocate(id);
    instanceMappingBuffer.Deallocate(id);

    drawCalls.erase(drawCalls.begin() + id);
    drawCallMaterialIndices.erase(drawCallMaterialIndices.begin() + id);

    // Fix pointers.
    for (int i = 0; i < batches.size(); i++) {
        bool update = false;
        if (batches[i].drawCall == -1U) continue;
        if (batches[i].drawCall > id) {
            batches[i].drawCall--;
            update = true;
        }

        for (int j = 0; j < 4; j++) {
            if (batches[i].lods[j] == -1U) break;
            if (batches[i].lods[j] > id) {
                batches[i].lods[j]--;
                update = true;
            }
        }

        if (update) batchBuffer.Write(i, batches[i]);
    }

    // Fix first instance.
    for (int i = id; i < drawCalls.size(); i++) {
        drawCalls[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(DrawCall, firstInstance));
    }
}
void BatchManager::AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize) {
    uint index = Get(mesh, material);
    if (index == -1U) index = Add(mesh, material, objectByteSize);

    renderObject->batchIndex = index;
    renderObject->objectDataIndex = renderObjects[index].size();

    if (GetObjectCount(index) + 1 >= GetObjectCapacity(index)) ReserveObjects(index, GetObjectCount(index) + 1);

    totalObjects++;
    renderObjects[index].push_back(renderObject);
    for (int i = index + 1; i < batches.size(); i++) {
        batches[i].firstObjectIndex++;
        batchBuffer.Write(i, batches[i].firstObjectIndex, offsetof(Batch, firstObjectIndex));
    }
}

void BatchManager::RemoveObject(RenderObject *renderObject) {
    auto index = renderObject->batchIndex;
    auto dataIndex = renderObject->objectDataIndex;

    if (renderObjects[index].size() > 0) {
        totalObjects--;
        std::swap(renderObjects[index][dataIndex], renderObjects[index][renderObjects[index].size() - 1]);
        renderObjects[index][dataIndex]->objectDataIndex = dataIndex;
        renderObjects[index].pop_back();

        // Move object and instance mapping data.
        char *data = new char[objectBuffer.Alignment(index)];
        objectBuffer.Read(
            index, data, objectBuffer.Alignment(index), objectBuffer.Alignment(index) * renderObjects[index].size()
        );
        objectBuffer.Write(index, data, objectBuffer.Alignment(index), objectBuffer.Alignment(index) * dataIndex);
        delete[] data;

        for (int i = index + 1; i < batches.size(); i++) {
            batches[i].firstObjectIndex--;
            batchBuffer.Write(i, batches[i].firstObjectIndex, offsetof(Batch, firstObjectIndex));
        }
    }

    if (renderObjects[index].size() == 0) Remove(index);
}

void BatchManager::NotifyMaterialDestroy(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &matManager = *currentRenderer->managers.materialManager;

    for (auto &material : drawCallMaterialIndices) {
        auto &[matIndex, variant] = material;
        assert(matIndex != index && "Can not destroy material that is being used.");
        if (matIndex > index) {
            matIndex--;
            uint id = &material - &drawCallMaterialIndices[0];
            drawCalls[id].materialIndex =
                matManager.materialBuffer.Offset(matIndex) / matManager.materialBuffer.Alignment(matIndex) + variant;

            drawCallBuffer.Write(id, drawCalls[id].materialIndex, offsetof(DrawCall, materialIndex));
        }
    }
}

void BatchManager::NotifyVariantDestroy(uint materialIndex, uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &matManager = *currentRenderer->managers.materialManager;

    for (auto &material : drawCallMaterialIndices) {
        auto &[matIndex, variant] = material;
        if (matIndex != materialIndex) continue;

        assert(variant != index && "Can not destroy material variant that is being used.");
        if (variant > index) {
            variant--;
            uint id = &material - &drawCallMaterialIndices[0];
            drawCalls[id].materialIndex =
                matManager.materialBuffer.Offset(matIndex) / matManager.materialBuffer.Alignment(matIndex) + variant;

            drawCallBuffer.Write(id, drawCalls[id].materialIndex, offsetof(DrawCall, materialIndex));
        }
    }
}

void BatchManager::NotifyMeshDestroy(uint index) {
    for (auto &drawCall : drawCalls) {
        assert(drawCall.meshIndex != index && "Can not destroy mesh that is being used.");

        if (drawCall.meshIndex > index) {
            drawCall.meshIndex--;
            drawCallBuffer.Write(index, drawCall.meshIndex, offsetof(DrawCall, meshIndex));
        }
    }
}
