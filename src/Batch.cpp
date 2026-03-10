#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include <algorithm>
using namespace vg;

BatchArray *BatchArray::batchArray = nullptr;

bool BatchArray::Exists(Mesh *mesh, Material *material) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_Exists(mesh, material);
}

uint BatchArray::Add(Mesh *mesh, Material *material, uint objectByteSize) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_Add(mesh, material, objectByteSize);
}

uint BatchArray::Get(Mesh *mesh, Material *material) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_Get(mesh, material);
}

void BatchArray::Remove(uint batchIndex) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_Remove(batchIndex);
}

void BatchArray::SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_SetLOD(batchIndex, lods);
}

void BatchArray::ReserveObjects(uint batchIndex, uint objectCount) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_ReserveObjects(batchIndex, objectCount);
}

void BatchArray::ShrinkToFit(uint batchIndex) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_ShrinkToFit(batchIndex);
}

uint BatchArray::GetObjectCapacity(uint batchIndex) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_GetObjectCapacity(batchIndex);
}

uint BatchArray::GetObjectCount(uint batchIndex) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    return batchArray->_GetObjectCount(batchIndex);
}
uint BatchArray::GetTotalInstanceCount() {
    assert(batchArray && "Current batchArray needs to be assigned!");
    uint sum = 0;
    for (auto c : batchArray->drawCallInstanceCount) sum += c;
    return sum;
}

BatchArray::BatchArray() {}

BatchArray::BatchArray(int maxFramesInFlight, uint transparencyBucketCount)
    : transparencyBucketCount(transparencyBucketCount) {
    drawCallBuffer = RenderBuffer(maxFramesInFlight, {BufferUsage::StorageBuffer, BufferUsage::IndirectBuffer}, 0);
    batchBuffer = RenderBuffer(maxFramesInFlight, BufferUsage::StorageBuffer, 0);
    objectBuffer = RenderBuffer(maxFramesInFlight, BufferUsage::StorageBuffer, 0);
}

BatchArray::BatchArray(BatchArray &&o) : BatchArray() {
    std::swap(transparencyBucketCount, o.transparencyBucketCount);
    std::swap(firstTransparentDrawCall, o.firstTransparentDrawCall);
    std::swap(transparentDrawCallsCount, o.transparentDrawCallsCount);
    std::swap(drawCalls, o.drawCalls);
    std::swap(drawCallMaterialIndices, o.drawCallMaterialIndices);
    std::swap(drawCallBuffer, o.drawCallBuffer);
    std::swap(batches, o.batches);
    std::swap(totalObjects, o.totalObjects);
    std::swap(renderObjects, o.renderObjects);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(objectBuffer, o.objectBuffer);
}

BatchArray &BatchArray::operator=(BatchArray &&o) {
    if (this == &o) return *this;

    std::swap(transparencyBucketCount, o.transparencyBucketCount);
    std::swap(firstTransparentDrawCall, o.firstTransparentDrawCall);
    std::swap(transparentDrawCallsCount, o.transparentDrawCallsCount);
    std::swap(drawCalls, o.drawCalls);
    std::swap(drawCallMaterialIndices, o.drawCallMaterialIndices);
    std::swap(drawCallBuffer, o.drawCallBuffer);
    std::swap(batches, o.batches);
    std::swap(totalObjects, o.totalObjects);
    std::swap(renderObjects, o.renderObjects);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(objectBuffer, o.objectBuffer);

    return *this;
}

BatchArray::~BatchArray() {}

bool BatchArray::_Exists(Mesh *mesh, Material *material) { return Get(mesh, material) != -1U; }

uint BatchArray::_Add(Mesh *mesh, Material *material, uint objectByteSize) {
    uint index = GetDrawCall(mesh, material);
    bool isTransparent = material->IsTransparent();

    if (index == -1U) {
        std::tuple<Material *, Mesh *> key(material, mesh);
        std::span<PartialDrawCall> search = {drawCalls.begin(), drawCalls.begin() + firstTransparentDrawCall};
        if (isTransparent)
            search = {
                drawCalls.begin() + firstTransparentDrawCall,
                drawCalls.begin() + firstTransparentDrawCall + transparentDrawCallsCount
            };

        auto it = std::lower_bound(search.begin(), search.end(), key, [](auto &a, auto &b) { return a < b; });
        index = (it - search.begin()) + isTransparent * firstTransparentDrawCall;
    }

    InsertDrawCall(index, mesh, material);
    if (isTransparent) {
        transparentDrawCallsCount++;
        uint ind = index;
        for (int i = 0; i < transparencyBucketCount - 1; i++) {
            ind += transparentDrawCallsCount;
            InsertDrawCall(ind, mesh, material);
        }
    }

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

uint BatchArray::_Get(Mesh *mesh, Material *material) {
    std::tuple<Material *, Mesh *> key(material, mesh);
    for (int i = 0; i < batches.size(); i++)
        if (drawCalls[batches[i].drawCall] == key) return i;

    return -1U;
}

void BatchArray::_Remove(uint index) {
    assert(index < batches.size() && "Invalid BatchID.");
    auto &batch = batches[index];

    std::vector<uint> drawCallsToDelete = {batch.drawCall};
    for (auto &&i : batch.lods) {
        if (i == -1U) break;
        drawCallsToDelete.push_back(batch.lods[i]);
    }
    std::sort(drawCallsToDelete.begin(), drawCallsToDelete.end());

    int initialDrawCalls = drawCallsToDelete.size();
    for (int i = 1; i < transparencyBucketCount; i++) {
        for (int j = 0; j < initialDrawCalls; j++) {
            uint ind = drawCallsToDelete[j];
            if (ind < firstTransparentDrawCall) continue;
            drawCallsToDelete.push_back(ind + transparentDrawCallsCount * i);
        }
    }

    // If for any draw call there is someone referencing it, don't destroy it.
    for (auto &b : batches) {
        if (&b == &batch) continue;

        auto it = std::lower_bound(drawCallsToDelete.begin(), drawCallsToDelete.end(), b.drawCall);
        if (it != drawCallsToDelete.end() && *it == b.drawCall) drawCallsToDelete.erase(it);
        for (auto &lod : b.lods) {
            if (lod == -1U) break;

            auto it = std::lower_bound(drawCallsToDelete.begin(), drawCallsToDelete.end(), lod);
            if (it != drawCallsToDelete.end() && *it == b.drawCall) drawCallsToDelete.erase(it);
        }

        if (drawCallsToDelete.empty()) break;
    }
    for (auto drawCall = drawCallsToDelete.rbegin(); drawCall != drawCallsToDelete.rend(); ++drawCall)
        DeleteDrawCall(*drawCall);

    // Delete objects.

    int objectCount = renderObjects[index].size();
    for (int i = index + 1; i < batches.size(); i++) {
        for (int j = 0; j < renderObjects[i].size(); j++) renderObjects[i][j]->batchIndex--;

        batches[i].objectDataOffset = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        batches[i].firstObjectIndex -= objectCount;
        batchBuffer.Write(i, batches[i].objectDataOffset, offsetof(Batch, objectDataOffset));
        batchBuffer.Write(i, batches[i].firstObjectIndex, offsetof(Batch, firstObjectIndex));
    }
    objectBuffer.Deallocate(index);
    totalObjects -= objectCount;
    renderObjects.erase(renderObjects.begin() + index);

    // Delete batch
    batchBuffer.Deallocate(index);
    batches.erase(batches.begin() + index);
}

void BatchArray::_SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods) {
    assert(batchIndex < batches.size() && "Invalid Batch ID.");
    assert(lods.size() < 4 && "LOD count has to be less than 4.");
    assert(batches[batchIndex].lods[0] == -1U && "LOD changing has to be implemented.");

    auto &batch = batches[batchIndex];
    for (int i = 0; i < lods.size(); i++) {
        auto &&[mesh, material] = lods[i];
        uint index = GetDrawCall(mesh, material);

        bool isTransparent = material->IsTransparent();

        if (index == -1U) {
            std::tuple<Material *, Mesh *> key(material, mesh);
            std::span<PartialDrawCall> search = {drawCalls.begin(), drawCalls.begin() + firstTransparentDrawCall};
            if (isTransparent)
                search = {
                    drawCalls.begin() + firstTransparentDrawCall,
                    drawCalls.begin() + firstTransparentDrawCall + transparentDrawCallsCount
                };

            auto it = std::lower_bound(search.begin(), search.end(), key, [](auto &a, auto &b) { return a < b; });
            index = (it - search.begin()) + isTransparent * firstTransparentDrawCall;
        }
        InsertDrawCall(index, mesh, material);
        if (isTransparent) {
            uint ind = index;
            for (int i = 0; i < transparencyBucketCount - 1; i++) {
                ind += transparentDrawCallsCount;
                InsertDrawCall(ind, mesh, material);
            }
        }

        batch.lods[i] = index;
    }
}

void BatchArray::_ReserveObjects(uint index, uint objectCount) {
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
    drawCallInstanceCount[batch.drawCall] = objectCount;
    if (batch.drawCall >= firstTransparentDrawCall) {
        uint ind = batch.drawCall;
        for (int i = 0; i < transparencyBucketCount - 1; i++) {
            ind += transparentDrawCallsCount;
            drawCallInstanceCount[ind] = objectCount;
        }
    }

    for (auto &lod : batch.lods) {
        if (lod == -1U) break;

        drawCallInstanceCount[lod] = objectCount;
        if (lod >= firstTransparentDrawCall) {
            uint ind = lod;
            for (int i = 0; i < transparencyBucketCount - 1; i++) {
                ind += transparentDrawCallsCount;

                drawCallInstanceCount[ind] = objectCount;
            }
        }
        minDrawCallID = std::min(minDrawCallID, lod);
    }

    for (int i = minDrawCallID + 1; i < drawCalls.size(); i++) {
        drawCalls[i].firstInstance = drawCalls[i - 1].firstInstance + drawCallInstanceCount[i - 1];
        drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(PartialDrawCall, firstInstance));
    }
}

void BatchArray::_ShrinkToFit(uint index) {
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

uint BatchArray::_GetObjectCapacity(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return drawCallInstanceCount[index];
}

uint BatchArray::_GetObjectCount(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return renderObjects[index].size();
}

bool BatchArray::PartialDrawCall::operator==(const std::tuple<Material *, Mesh *> &o) const {
    assert(batchArray && "Current batchArray needs to be assigned!");

    auto id = this - &batchArray->drawCalls[0];
    auto [index, variant] = batchArray->drawCallMaterialIndices[id];
    return index == std::get<Material *>(o)->index && variant == std::get<Material *>(o)->variant &&
           meshIndex == std::get<Mesh *>(o)->index;
}
bool BatchArray::PartialDrawCall::operator<(const std::tuple<Material *, Mesh *> &o) const {
    assert(batchArray && "Current batchArray needs to be assigned!");

    auto id = this - &batchArray->drawCalls[0];
    auto [index, variant] = batchArray->drawCallMaterialIndices[id];

    if (index < std::get<Material *>(o)->index) return true;
    if (index > std::get<Material *>(o)->index) return false;

    if (variant < std::get<Material *>(o)->variant) return true;
    if (variant > std::get<Material *>(o)->variant) return false;
    return meshIndex < std::get<Mesh *>(o)->index;
}

uint BatchArray::GetDrawCall(Mesh *mesh, Material *material) {
    assert(batchArray && "Current batchArray needs to be assigned!");
    std::tuple<Material *, Mesh *> key(material, mesh);
    auto it = std::find(drawCalls.begin(), drawCalls.end(), key);
    if (it == drawCalls.end()) return -1U;
    return it - drawCalls.begin();
}

void BatchArray::InsertDrawCall(uint index, Mesh *mesh, Material *material) {
    assert(batchArray && "Current batchArray needs to be assigned!");

    RenderBuffer &materialBuffer = Material::materialArray->materialBuffer;
    drawCallBuffer.Allocate(sizeof(PartialDrawCall), sizeof(PartialDrawCall), index);
    drawCallInstanceCount.insert(drawCallInstanceCount.begin() + index, 0);

    PartialDrawCall drawCall;
    drawCall.firstInstance = index > 0 ? drawCalls[index - 1].firstInstance + drawCallInstanceCount[index - 1] : 0;
    drawCall.meshIndex = mesh->index;
    drawCall.materialIndex = 0;
    if (materialBuffer.Alignment(material->index) != 0)
        drawCall.materialIndex =
            materialBuffer.Offset(material->index) / materialBuffer.Alignment(material->index) + material->variant;
    drawCallBuffer.Write(index, drawCall);

    drawCalls.emplace(drawCalls.begin() + index, std::move(drawCall));
    drawCallMaterialIndices.insert(drawCallMaterialIndices.begin() + index, {material->index, material->variant});

    if (!material->IsTransparent()) firstTransparentDrawCall++;

    // Fix pointers.
    for (int i = 0; i < batches.size(); i++) {
        bool update = false;
        if (batches[i].drawCall == -1U) continue;
        if (batches[i].drawCall >= index) {
            batches[i].drawCall++;
            update = true;
        }

        for (int j = 0; j < 4; j++) {
            if (batches[i].lods[j] == -1U) break;
            if (batches[i].lods[j] >= index) {
                batches[i].lods[j]++;
                update = true;
            }
        }

        if (update) batchBuffer.Write(i, batches[i]);
    }

    // Fix first instance.
    for (int i = index + 1; i < drawCalls.size(); i++) {
        drawCalls[i].firstInstance = drawCalls[i - 1].firstInstance + drawCallInstanceCount[i - 1];
        drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(PartialDrawCall, firstInstance));
    }
}

void BatchArray::DeleteDrawCall(uint id) {
    if (id < firstTransparentDrawCall) firstTransparentDrawCall--;
    else if (id < firstTransparentDrawCall + transparentDrawCallsCount) transparentDrawCallsCount--;

    drawCallBuffer.Deallocate(id);
    drawCallInstanceCount.erase(drawCallInstanceCount.begin() + id);

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
        if (i == 0) drawCalls[i].firstInstance = 0;
        else drawCalls[i].firstInstance = drawCalls[i - 1].firstInstance + drawCallInstanceCount[i - 1];
        drawCallBuffer.Write(i, drawCalls[i].firstInstance, offsetof(PartialDrawCall, firstInstance));
    }
}

void BatchArray::AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize) {
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

void BatchArray::RemoveObject(RenderObject *renderObject) {
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

void BatchArray::NotifyMaterialDestroy(uint index) {
    assert(Material::materialArray && "Current materialArray needs to be assigned!");

    for (auto &material : drawCallMaterialIndices) {
        auto &[matIndex, variant] = material;
        // assert(matIndex != index && "Can not destroy material that is being used.");
        if (matIndex > index) {
            matIndex--;
            uint id = &material - &drawCallMaterialIndices[0];
            drawCalls[id].materialIndex = Material::materialArray->materialBuffer.Offset(matIndex) /
                                              Material::materialArray->materialBuffer.Alignment(matIndex) +
                                          variant;

            drawCallBuffer.Write(id, drawCalls[id].materialIndex, offsetof(PartialDrawCall, materialIndex));
        }
    }
}

void BatchArray::NotifyVariantDestroy(uint materialIndex, uint index) {
    assert(Material::materialArray && "Current batchArray needs to be assigned!");

    for (auto &material : drawCallMaterialIndices) {
        auto &[matIndex, variant] = material;
        if (matIndex != materialIndex) continue;

        // assert(variant != index && "Can not destroy material variant that is being used.");
        if (variant > index) {
            variant--;
            uint id = &material - &drawCallMaterialIndices[0];
            drawCalls[id].materialIndex = Material::materialArray->materialBuffer.Offset(matIndex) /
                                              Material::materialArray->materialBuffer.Alignment(matIndex) +
                                          variant;

            drawCallBuffer.Write(id, drawCalls[id].materialIndex, offsetof(PartialDrawCall, materialIndex));
        }
    }
}

void BatchArray::NotifyMeshDestroy(uint index) {
    for (auto &drawCall : drawCalls) {
        // assert(drawCall.meshIndex != index && "Can not destroy mesh that is being used.");

        if (drawCall.meshIndex > index) {
            drawCall.meshIndex--;
            drawCallBuffer.Write(index, drawCall.meshIndex, offsetof(PartialDrawCall, meshIndex));
        }
    }
}
