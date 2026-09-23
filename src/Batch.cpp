#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include "DrawCallArray.h"
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
    return batchArray->drawCallArray.GetTotalInstanceCount();
}

BatchArray::BatchArray() {}

BatchArray::BatchArray(int maxFramesInFlight, uint transparencyBucketCount)
    : drawCallArray(maxFramesInFlight, transparencyBucketCount) {
    batchBuffer = RenderBuffer(maxFramesInFlight, BufferUsage::StorageBuffer, 0);
    objectBuffer = RenderBuffer(maxFramesInFlight, BufferUsage::StorageBuffer, 0);
}

BatchArray::BatchArray(BatchArray &&o) : BatchArray() {
    std::swap(drawCallArray, o.drawCallArray);
    std::swap(batches, o.batches);
    std::swap(totalObjects, o.totalObjects);
    std::swap(renderObjects, o.renderObjects);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(objectBuffer, o.objectBuffer);
}

BatchArray &BatchArray::operator=(BatchArray &&o) {
    if (this == &o) return *this;
    std::swap(drawCallArray, o.drawCallArray);
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
    uint index = drawCallArray.GetDrawCall(mesh, material);
    if (index == -1U) index = drawCallArray.GetInsertionIndex(mesh, material);
    drawCallArray.InsertDrawCall(index, mesh, material);

    // Create Batch.
    batchBuffer.Allocate(sizeof(Batch), sizeof(Batch));
    objectBuffer.Allocate(0, objectByteSize);

    Batch batch;
    batch.objectDataOffset = objectBuffer.Offset(batches.size()) / std::max(objectBuffer.Alignment(batches.size()), 1u);
    batch.firstObjectIndex = totalObjects;
    batch.objectDataElementSize = objectByteSize;
    batch.drawCall = index;
    for (int i = 0; i < std::size(batch.lods); i++) batch.lods[i] = -1U;
    batchBuffer.Write(batches.size(), batch);
    batches.emplace_back(std::move(batch));
    renderObjects.emplace_back(std::vector<RenderObject *>());

    return batches.size() - 1;
}

uint BatchArray::_Get(Mesh *mesh, Material *material) const {
    std::tuple<const DrawCallArray *, const Material *, const Mesh *> key(&drawCallArray, material, mesh);
    for (int i = 0; i < batches.size(); i++)
        if (drawCallArray.drawCallReferences[batches[i].drawCall] == key) return i;

    return -1U;
}

void BatchArray::_Remove(uint index) {
    assert(index < batches.size() && "Invalid BatchID.");
    auto &batch = batches[index];

    // Get all draw calls to delete.
    std::vector<uint> drawCallsToDelete = {batch.drawCall};
    for (auto &&i : batch.lods) {
        if (i == -1U) break;
        drawCallsToDelete.push_back(i);
    }
    std::sort(drawCallsToDelete.begin(), drawCallsToDelete.end());

    // Also add transparent draw calls.
    int initialDrawCalls = drawCallsToDelete.size();
    for (int i = 1; i < drawCallArray.transparencyBucketCount; i++) {
        for (int j = 0; j < initialDrawCalls; j++) {
            uint ind = drawCallsToDelete[j];
            if (ind < drawCallArray.firstTransparentIndex) continue;
            drawCallsToDelete.push_back(ind + drawCallArray.transparentCount * i);
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
        drawCallArray.DeleteDrawCall(*drawCall);

    // Delete objects.
    int objectCount = renderObjects[index].size();
    for (int i = index + 1; i < batches.size(); i++) {
        for (int j = 0; j < renderObjects[i].size(); j++) renderObjects[i][j]->batchIndex--;

        batches[i].objectDataOffset = objectBuffer.Offset(i) / std::max(objectBuffer.Alignment(i), 1u);
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
    assert(lods.size() <= 4 && "LOD count has to be less than or equal 4.");
    assert(batches[batchIndex].lods[0] == -1U && "LOD changing has to be implemented.");

    int objectCapacity = GetObjectCapacity(batchIndex);
    auto &batch = batches[batchIndex];
    for (int i = 0; i < lods.size(); i++) {
        auto &&[mesh, material] = lods[i];
        uint index = drawCallArray.GetDrawCall(mesh, material);
        if (index == -1U) index = drawCallArray.GetInsertionIndex(mesh, material);
        drawCallArray.InsertDrawCall(index, mesh, material);

        batch.lods[i] = index;
        batchBuffer.Write(batchIndex, batch.lods, offsetof(Batch, lods));
    }
    _ReserveObjects(batchIndex, objectCapacity);
}

void BatchArray::_ReserveObjects(uint index, uint objectCount) {
    assert(index < batches.size() && "Invalid Batch ID.");
    auto &batch = batches[index];

    // Reserve objects for batch.
    renderObjects[index].reserve(objectCount);
    objectBuffer.Reallocate(index, objectBuffer.Alignment(index) * objectCount);
    for (int i = index + 1; i < batches.size(); i++) {
        batches[i].objectDataOffset = objectBuffer.Offset(i) / std::max(objectBuffer.Alignment(i), 1u);
        batchBuffer.Write(i, batches[i].objectDataOffset, offsetof(Batch, objectDataOffset));
    }

    // Reserve objects for drawCalls.
    uint minDrawCallID = batch.drawCall;
    drawCallArray.drawCallData[batch.drawCall].instanceCapacity = objectCount;
    if (batch.drawCall >= drawCallArray.firstTransparentIndex) {
        uint ind = batch.drawCall;
        for (int i = 0; i < drawCallArray.transparencyBucketCount - 1; i++) {
            ind += drawCallArray.transparentCount;
            drawCallArray.drawCallData[ind].instanceCapacity = objectCount;
        }
    }

    for (auto &lod : batch.lods) {
        if (lod == -1U) break;

        drawCallArray.drawCallData[lod].instanceCapacity = objectCount;
        if (lod >= drawCallArray.firstTransparentIndex) {
            uint ind = lod;
            for (int i = 0; i < drawCallArray.transparencyBucketCount - 1; i++) {
                ind += drawCallArray.transparentCount;
                drawCallArray.drawCallData[ind].instanceCapacity = objectCount;
            }
        }
        minDrawCallID = std::min(minDrawCallID, lod);
    }

    drawCallArray.UpdateFirstInstance(minDrawCallID + 1, drawCallArray.drawCallReferences.size() - 1);
}

void BatchArray::_ShrinkToFit(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    uint objectCount = GetObjectCount(index);
    uint objectCapacity = GetObjectCapacity(index);
    if (objectCount >= objectCapacity) return;

    ReserveObjects(index, objectCount);
    if (objectCount == 0) Remove(index);
}

uint BatchArray::_GetObjectCapacity(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return drawCallArray.drawCallData[batches[index].drawCall].instanceCapacity;
}

uint BatchArray::_GetObjectCount(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return renderObjects[index].size();
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

    // if (renderObjects[index].size() == 0) Remove(index);
}

void BatchArray::NotifyDrawCallInsert(uint index) {
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
}

void BatchArray::NotifyDrawCallDestroy(uint index) {
    for (int i = 0; i < batches.size(); i++) {
        bool update = false;
        if (batches[i].drawCall == -1U) continue;
        if (batches[i].drawCall > index) {
            batches[i].drawCall--;
            update = true;
        }

        for (int j = 0; j < 4; j++) {
            if (batches[i].lods[j] == -1U) break;
            if (batches[i].lods[j] > index) {
                batches[i].lods[j]--;
                update = true;
            }
        }

        if (update) batchBuffer.Write(i, batches[i]);
    }
}
