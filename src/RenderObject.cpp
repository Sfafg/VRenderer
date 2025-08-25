#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Buffer.h"
#include <algorithm>

Batch::Batch(class Material *material, class Mesh *mesh) : material(material), mesh(mesh) {}

Batch::Batch() : material(nullptr), mesh(nullptr) {}

Batch::Batch(Batch &&o) : Batch() {
    std::swap(material, o.material);
    std::swap(mesh, o.mesh);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(renderObjects, o.renderObjects);

    if (material) {
        auto it = std::find(material->batches.begin(), material->batches.end(), &o);
        if (it != material->batches.end()) *it = this;
    }
    if (mesh) {
        auto it = std::find(mesh->batches.begin(), mesh->batches.end(), &o);
        if (it != mesh->batches.end()) *it = this;
    }
}

Batch &Batch::operator=(Batch &&o) {
    if (this == &o) return *this;

    std::swap(material, o.material);
    std::swap(mesh, o.mesh);
    std::swap(batchBuffer, o.batchBuffer);
    std::swap(renderObjects, o.renderObjects);

    if (material) {
        auto it = std::find(material->batches.begin(), material->batches.end(), &o);
        if (it != material->batches.end()) *it = this;
    }
    if (o.material) {
        auto it = std::find(o.material->batches.begin(), o.material->batches.end(), this);
        if (it != o.material->batches.end()) *it = &o;
    }
    if (mesh) {
        auto it = std::find(mesh->batches.begin(), mesh->batches.end(), &o);
        if (it != mesh->batches.end()) *it = this;
    }
    if (o.mesh) {
        auto it = std::find(o.mesh->batches.begin(), o.mesh->batches.end(), this);
        if (it != o.mesh->batches.end()) *it = &o;
    }

    return *this;
}

Batch::~Batch() {
    if (material) {
        auto it = std::find(material->batches.begin(), material->batches.end(), this);
        if (it != material->batches.end()) material->batches.erase(it);
    }
    if (mesh) {
        auto it = std::find(mesh->batches.begin(), mesh->batches.end(), this);
        if (it != mesh->batches.end()) mesh->batches.erase(it);
    }
}

bool Batch::operator==(const Batch &o) const {
    return material->index == o.material->index && material->variant == o.material->variant &&
           mesh->index == o.mesh->index;
}

bool Batch::operator<(const Batch &o) const {
    if (material->index < o.material->index) return true;
    if (material->index > o.material->index) return false;

    if (material->variant < o.material->variant) return true;
    if (material->variant > o.material->variant) return false;
    return mesh->index < o.mesh->index;
}

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t batchDataByteSize, const void *data) {
    Batch t{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), t);

    batchIndex = (it - batches.begin());
    if (it == batches.end() || *it != t) {
        t.batchBuffer = RenderBuffer(batchDataByteSize + (batchDataByteSize == 0), vg::BufferUsage::VertexBuffer);
        batches.emplace(it, std::move(t));

        for (int i = batchIndex + 1; i < batches.size(); i++)
            for (auto &&ro : batches[i].renderObjects) ro->batchIndex++;
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

    auto &batch = GetBatch();
    batch.batchBuffer.Deallocate(batchDataIndex);
    batch.renderObjects.erase(batch.renderObjects.begin() + batchDataIndex);
    for (int i = batchDataIndex; i < batch.renderObjects.size(); i++) batch.renderObjects[i]->batchDataIndex--;

    if (batch.renderObjects.size() == 0) {
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
