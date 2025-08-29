#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Buffer.h"

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t batchDataByteSize, const void *data) {
    std::tuple t{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), t, [](const auto &a, const auto &b) { return a < b; });

    batchIndex = (it - batches.begin());
    if (it == batches.end() || *it != t) {
        assert(it == batches.end() && "TODO: This should insert a new region, not append it");

        batchIndex = objectDataBuffer.Allocate(0, batchDataByteSize);
        batches.emplace(it, material, mesh, batchDataByteSize);

        for (int i = batchIndex + 1; i < batches.size(); i++)
            for (auto &&ro : batches[i].renderObjects) ro->batchIndex++;
    }
    batches[batchIndex].renderObjects.push_back(this);

    instanceMapping.Allocate(sizeof(int), sizeof(int));
    batchDataIndex = 0;
    if (batchDataByteSize > 0)
        batchDataIndex = objectDataBuffer.Size(batchIndex) / objectDataBuffer.Alignment(batchIndex);
    objectDataBuffer.Reallocate(batchIndex, objectDataBuffer.Size(batchIndex) + batchDataByteSize);
    if (data) objectDataBuffer.Write(batchIndex, data, batchDataByteSize, batchDataIndex * batchDataByteSize);
    int a = objectDataBuffer.Offset(batchIndex) / objectDataBuffer.Alignment(batchIndex);
    batches[batchIndex].batchMetaData.Write(batchIndex, a);
}

RenderObject::RenderObject() : batchIndex(-1U), batchDataIndex(0) {}

RenderObject::RenderObject(RenderObject &&o) {
    std::swap(batchIndex, o.batchIndex);
    std::swap(batchDataIndex, o.batchDataIndex);
    if (batchIndex != -1U) batches[batchIndex].renderObjects[batchDataIndex] = this;
}

RenderObject &RenderObject::operator=(RenderObject &&o) {
    if (this == &o) return *this;

    if (batchIndex != -1U && o.batchIndex != -1U) {
        std::swap(
            batches[batchIndex].renderObjects[batchDataIndex], batches[o.batchIndex].renderObjects[o.batchDataIndex]
        );
    } else if (o.batchIndex != -1U) batches[o.batchIndex].renderObjects[o.batchDataIndex] = this;

    std::swap(batchIndex, o.batchIndex);
    std::swap(batchDataIndex, o.batchDataIndex);

    return *this;
}

RenderObject::~RenderObject() {
    if (batchIndex == -1U) return;
    auto &batch = GetBatch();

    batch.renderObjects.erase(batch.renderObjects.begin() + batchDataIndex);
    if (batch.renderObjects.size() == 0) {
        objectDataBuffer.Deallocate(batchIndex);
        batches.erase(batches.begin() + batchIndex);
        for (int i = batchIndex; i < batches.size(); i++)
            for (auto &j : batches[i].renderObjects) j->batchIndex--;
    } else {
        objectDataBuffer.Erase(
            batchIndex, objectDataBuffer.Alignment(batchIndex), batchDataIndex * objectDataBuffer.Alignment(batchIndex)
        );
        for (int i = batchDataIndex; i < batch.renderObjects.size(); i++) batch.renderObjects[i]->batchDataIndex--;
    }
    batchIndex = -1U;
}

Batch &RenderObject::GetBatch() { return batches[batchIndex]; }
const Batch &RenderObject::GetBatch() const { return batches[batchIndex]; }

void RenderObject::SetBatchData(const void *data, uint32_t byteSize) {
    objectDataBuffer.Write(batchIndex, data, byteSize, objectDataBuffer.Alignment(batchIndex) * batchDataIndex);
}

void RenderObject::ReadBatchData(void *data) {
    objectDataBuffer.Read(
        batchIndex, data, objectDataBuffer.Alignment(batchIndex),
        objectDataBuffer.Alignment(batchIndex) * batchDataIndex
    );
}

std::vector<Batch> RenderObject::batches;
RenderBuffer RenderObject::objectDataBuffer(vg::BufferUsage::StorageBuffer);
RenderBuffer RenderObject::instanceMapping({vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer});
