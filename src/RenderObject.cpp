#include "RenderObject.h"

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t batchDataByteSize, const void *data) {
    Batch t{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), t, [](const auto &a, const auto &b) { return a < b; });

    batchIndex = (it - batches.begin());
    if (it == batches.end() || *it != t) {
        batches.insert(it, t);
        batchBuffers.emplace(
            batchBuffers.begin() + batchIndex,
            RenderBuffer(batchDataByteSize + (batchDataByteSize == 0), vg::BufferUsage::VertexBuffer)
        );
    }
    batches[batchIndex].instanceCount++;
    renderObjects.push_back(this);

    if (batchDataByteSize != 0) {
        batchDataIndex = batchBuffers[batchIndex].Allocate(batchDataByteSize, batchDataByteSize);
        if (data) batchBuffers[batchIndex].Write(batchDataIndex, data, batchDataByteSize);
    }
}

RenderObject::RenderObject() : batchIndex(-1U), batchDataIndex(0) {}

RenderObject::RenderObject(RenderObject &&o) {
    std::swap(batchIndex, o.batchIndex);
    std::swap(batchDataIndex, o.batchDataIndex);
    if (batchIndex != -1U) renderObjects[batchIndex] = this;
}

RenderObject &RenderObject::operator=(RenderObject &&o) {
    if (this == &o) return *this;

    if (batchIndex != -1U && o.batchIndex != -1U) std::swap(renderObjects[batchIndex], renderObjects[o.batchIndex]);
    else if (o.batchIndex != -1U) renderObjects[o.batchIndex] = this;

    std::swap(batchIndex, o.batchIndex);
    std::swap(batchDataIndex, o.batchDataIndex);

    return *this;
}

RenderObject::~RenderObject() {
    if (batchIndex == -1U) return;

    batches[batchIndex].instanceCount--;
    batchBuffers[batchIndex].Deallocate(batchIndex);
    renderObjects.erase(renderObjects.begin() + batchIndex);
    for (int i = batchIndex; i < renderObjects.size(); i++) renderObjects[i]->batchDataIndex--;
    batchIndex = -1U;
}

void RenderObject::SetBatchData(const void *data, uint32_t byteSize) {
    auto &batchBuffer = batchBuffers[batchIndex];
    if (batchBuffer.Size(batchDataIndex) != byteSize) batchBuffer.Reallocate(batchDataIndex, byteSize);
    batchBuffer.Write(batchDataIndex, data, byteSize);
}

void RenderObject::ReadBatchData(void *data) {
    auto &batchBuffer = batchBuffers[batchIndex];
    batchBuffer.Read(batchDataIndex, data);
}

std::vector<Batch> RenderObject::batches;
std::vector<RenderBuffer> RenderObject::batchBuffers;
std::vector<RenderObject *> RenderObject::renderObjects;
