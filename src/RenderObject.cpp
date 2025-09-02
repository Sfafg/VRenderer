#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Buffer.h"

RenderObject::RenderObject(Mesh *mesh, Material *material, uint32_t objectByteSize, const void *data) {
    Batch::AddObject(this, mesh, material, objectByteSize);
    if (data) Batch::objectBuffer.Write(batchIndex, data, objectByteSize, objectDataIndex * objectByteSize);
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
            Batch::batches[batchIndex].renderObjects[objectDataIndex],
            Batch::batches[o.batchIndex].renderObjects[o.objectDataIndex]
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

Batch &RenderObject::GetBatch() { return Batch::batches[batchIndex]; }
const Batch &RenderObject::GetBatch() const { return Batch::batches[batchIndex]; }

void RenderObject::SetBatchData(const void *data, uint32_t byteSize) {
    Batch::objectBuffer.Write(batchIndex, data, byteSize, Batch::objectBuffer.Alignment(batchIndex) * objectDataIndex);
}

void RenderObject::ReadBatchData(void *data) {
    Batch::objectBuffer.Read(
        batchIndex, data, Batch::objectBuffer.Alignment(batchIndex),
        Batch::objectBuffer.Alignment(batchIndex) * objectDataIndex
    );
}
