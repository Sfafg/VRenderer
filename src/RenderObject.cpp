#include "RenderObject.h"
#include "Buffer.h"

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t batchDataByteSize, const void *data) {
    Batch t{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), t);

    batchIndex = (it - batches.begin());
    if (it == batches.end() || *it != t) {
        t.batchBuffer = RenderBuffer(batchDataByteSize + (batchDataByteSize == 0), vg::BufferUsage::VertexBuffer);
        batches.emplace(it, std::move(t));

        for (int i = batchIndex + 1; i < batches.size(); i++)
            for (auto &&ro : batches[i].renderObjects) ro->batchIndex--;
    }
    batches[batchIndex].renderObjects.push_back(this);

    batchDataIndex = batches[batchIndex].batchBuffer.Allocate(batchDataByteSize, batchDataByteSize);
    if (data) batches[batchIndex].batchBuffer.Write(batchDataIndex, data, batchDataByteSize);
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

    batches[batchIndex].batchBuffer.Deallocate(batchDataIndex);
    batches[batchIndex].renderObjects.erase(batches[batchIndex].renderObjects.begin() + batchDataIndex);
    for (int i = batchDataIndex; i < batches[batchIndex].renderObjects.size(); i++)
        batches[batchIndex].renderObjects[i]->batchDataIndex--;

    if (batches[batchIndex].renderObjects.size() == 0) {
        batches.erase(batches.begin() + batchIndex);
        for (int i = batchIndex; i < batches.size(); i++)
            for (auto &&ro : batches[i].renderObjects) ro->batchIndex--;
    }

    batchIndex = -1U;
}

Batch &RenderObject::GetBatch() { return batches[batchIndex]; }
const Batch &RenderObject::GetBatch() const { return batches[batchIndex]; }

void RenderObject::SetBatchData(const void *data, uint32_t byteSize) {
    auto &batchBuffer = batches[batchIndex].batchBuffer;
    if (batchBuffer.Size(batchDataIndex) != byteSize) batchBuffer.Reallocate(batchDataIndex, byteSize);
    batchBuffer.Write(batchDataIndex, data, byteSize);
}

void RenderObject::ReadBatchData(void *data) {
    auto &batchBuffer = batches[batchIndex].batchBuffer;
    batchBuffer.Read(batchDataIndex, data);
}

std::vector<Batch> RenderObject::batches;
