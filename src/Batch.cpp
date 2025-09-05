#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include <algorithm>

Batch::Batch() {}
uint Batch::LodCount() const { return lodCountPointerParent >> 28 & 0b1111; }
uint Batch::LodPointer() const { return lodCountPointerParent >> 14 & 0b11111111111111; }
uint Batch::ParentPointer() const { return lodCountPointerParent & 0b11111111111111; }
void Batch::SetLodCount(uint count) { lodCountPointerParent = (lodCountPointerParent & ~(0b1111 << 28)) | count << 28; }
void Batch::SetLodPointer(uint pointer) {
    lodCountPointerParent = (lodCountPointerParent & ~(0b11111111111111 << 14)) | pointer << 14;
}
void Batch::SetParentPointer(uint pointer) {
    lodCountPointerParent = (lodCountPointerParent & ~0b11111111111111) | pointer;
}

Batch *Batch::GetNextLod() const {
    auto lod = LodPointer();
    if (lod == 0b11111111111111) return nullptr;
    return &batches[lod];
}
Batch *Batch::GetLodParent() const {
    auto lod = ParentPointer();
    if (lod == 0b11111111111111) return nullptr;
    return &batches[lod];
}

std::tuple<uint, bool> Batch::Add(Material *material, Mesh *mesh, uint objectByteSize, bool isLod) {
    auto [index, exists] = Get(material, mesh);

    if (!exists) {
        uint instanceMappingRegion = 0;
        if (!isLod) {
            for (int i = 0; i < index; i++)
                if (batches[i].GetLodParent() == nullptr) instanceMappingRegion++;
        } else instanceMappingRegion = batches.size();
        for (int i = 0; i < instanceMappingRegionIndex.size(); i++)
            if (instanceMappingRegionIndex[i] >= instanceMappingRegion) instanceMappingRegionIndex[i]++;

        Batch batch;
        batch.materialIndex =
            Material::materialBuffer.Offset(material->index) / Material::materialBuffer.Alignment(material->index) +
            material->variant;
        batch.meshIndex = mesh->index;
        batch.batchDataElementSize = objectByteSize;
        batch.lodCountPointerParent = NULL_LOD;

        batches.emplace(batches.begin() + index, std::move(batch));
        renderObjects.insert(renderObjects.begin() + index, std::vector<RenderObject *>());
        materialIndices.insert(materialIndices.begin() + index, {material->index, material->variant});
        instanceMappingRegionIndex.insert(instanceMappingRegionIndex.begin() + index, instanceMappingRegion);

        drawCallBuffer.Allocate(sizeof(Batch), sizeof(Batch), index);
        objectBuffer.Allocate(0, objectByteSize, index);
        instanceMappingBuffer.Allocate(0, sizeof(InstanceMapping), instanceMappingRegion);

        batch.firstInstance = instanceMappingBuffer.Offset(instanceMappingRegion) /
                              instanceMappingBuffer.Alignment(instanceMappingRegion);
        if (!isLod) batch.firstObject = objectBuffer.Offset(index) / objectBuffer.Alignment(index);
        drawCallBuffer.Write(index, batch);

        FixAfterBatchChange(index, 1);
    }

    return {index, exists};
}

std::tuple<uint, bool> Batch::Get(Material *material, Mesh *mesh) {
    std::tuple key{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), key, [](const auto &a, const auto &b) { return a < b; });
    bool exists = it != Batch::batches.end() && key == *it;
    uint index = it - batches.begin();
    return {index, exists};
}

void Batch::Remove(uint index) {
    for (int i = batches[index].LodCount(); i > 0; i--) {
        uint lodTail = index;
        for (int j = 0; j < i; j++) lodTail = batches[lodTail].LodPointer();
        Remove(lodTail);
        if (index > lodTail) index--;
    }

    uint objectCount = renderObjects[index].size();
    renderObjects[index].clear();
    instanceMappingBuffer.Deallocate(instanceMappingRegionIndex[index]);
    totalObjects -= objectCount;
    FixAfterObjectChange(index, 0, -objectCount);

    for (int i = 0; i < instanceMappingRegionIndex.size(); i++)
        if (instanceMappingRegionIndex[i] >= instanceMappingRegionIndex[index]) instanceMappingRegionIndex[i]--;
    batches.erase(batches.begin() + index);
    materialIndices.erase(materialIndices.begin() + index);
    renderObjects.erase(renderObjects.begin() + index);
    instanceMappingRegionIndex.erase(instanceMappingRegionIndex.begin() + index);

    drawCallBuffer.Deallocate(index);
    objectBuffer.Deallocate(index);

    FixAfterBatchChange(index, -1);
}

void Batch::ReserveObjects(uint batchIndex, uint objectCount) {
    instanceMappingBuffer.Reserve(objectCount * sizeof(InstanceMapping) * (batches[batchIndex].LodCount() + 1));
    objectBuffer.Reserve(objectCount * objectBuffer.Alignment(batchIndex));
}

void Batch::AddLOD(uint batchIndex, Material *material, Mesh *mesh) {
    auto [index, exists] = Add(material, mesh, batches[batchIndex].batchDataElementSize, true);
    assert(!exists || (batches[index].GetLodParent() != nullptr) && "Cannot set non LOD batch as LOD.");

    uint tailIndex = batchIndex;
    for (int i = 0; i < batches[batchIndex].LodCount(); i++) tailIndex = batches[tailIndex].LodPointer();
    batches[tailIndex].SetLodPointer(index);
    batches[batchIndex].SetLodCount(batches[batchIndex].LodCount() + 1);
    batches[index].SetParentPointer(batchIndex);
    batches[index].firstObject = objectBuffer.Offset(batchIndex) / objectBuffer.Alignment(batchIndex);

    drawCallBuffer.Write(tailIndex, batches[tailIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    drawCallBuffer.Write(batchIndex, batches[batchIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    drawCallBuffer.Write(index, batches[index].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    drawCallBuffer.Write(index, batches[index].firstObject, offsetof(Batch, firstObject));
    instanceMappingBuffer.Reallocate(index, instanceMappingBuffer.Size(instanceMappingRegionIndex[batchIndex]));
    FixAfterObjectChange(index, 0, renderObjects[batchIndex].size());
}

bool Batch::operator==(const std::tuple<Material *, Mesh *> &o) const {
    auto id = this - &batches[0];
    auto [index, variant] = materialIndices[id];
    return index == std::get<Material *>(o)->index && variant == std::get<Material *>(o)->variant &&
           meshIndex == std::get<Mesh *>(o)->index;
}

bool Batch::operator<(const std::tuple<Material *, Mesh *> &o) const {
    auto id = this - &batches[0];
    auto [index, variant] = materialIndices[id];
    if (index < std::get<Material *>(o)->index) return true;
    if (index > std::get<Material *>(o)->index) return false;

    if (variant < std::get<Material *>(o)->variant) return true;
    if (variant > std::get<Material *>(o)->variant) return false;
    return meshIndex < std::get<Mesh *>(o)->index;
}

void Batch::NotifyMaterialDestroy(uint index) {
    for (auto &material : materialIndices) {
        auto &[matIndex, variant] = material;
        assert(matIndex != index && "Can not destroy material that is being used.");
        if (matIndex > index) {
            matIndex--;
            uint id = &material - &materialIndices[0];
            batches[id].materialIndex =
                Material::materialBuffer.Offset(matIndex) / Material::materialBuffer.Alignment(matIndex) + variant;
            drawCallBuffer.Write(id, batches[id].materialIndex, offsetof(Batch, materialIndex));
        }
    }
}

void Batch::NotifyVariantDestroy(uint materialIndex, uint index) {
    for (auto &material : materialIndices) {
        auto &[matIndex, variant] = material;
        if (matIndex != materialIndex) continue;

        assert(variant != index && "Can not destroy material variant that is being used.");
        if (variant > index) {
            variant--;
            uint id = &material - &materialIndices[0];
            batches[id].materialIndex =
                Material::materialBuffer.Offset(matIndex) / Material::materialBuffer.Alignment(matIndex) + variant;
            drawCallBuffer.Write(id, batches[id].materialIndex, offsetof(Batch, materialIndex));
        }
    }
}

void Batch::NotifyMeshDestroy(uint index) {
    for (auto &batch : batches) {
        assert(batch.meshIndex != index && "Can not destroy mesh that is being used.");

        if (batch.meshIndex > index) {
            batch.meshIndex--;
            drawCallBuffer.Write(index, batch.meshIndex, offsetof(Batch, meshIndex));
        }
    }
}

void Batch::AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize) {
    auto [index, exists] = Add(material, mesh, objectByteSize);
    assert(batches[index].GetLodParent() == nullptr && "Cannot add objects to LOD batch.");
    renderObject->batchIndex = index;
    renderObject->objectDataIndex = renderObjects[index].size();
    totalObjects++;

    renderObjects[index].push_back(renderObject);
    objectBuffer.Reallocate(index, objectBuffer.Size(index) + objectByteSize);
    instanceMappingBuffer.Reallocate(
        instanceMappingRegionIndex[index],
        instanceMappingBuffer.Size(instanceMappingRegionIndex[index]) + sizeof(InstanceMapping)
    );

    uint tailIndex = index;
    while ((tailIndex = batches[tailIndex].LodPointer()) != 0b11111111111111) {
        instanceMappingBuffer.Reallocate(
            instanceMappingRegionIndex[tailIndex], instanceMappingBuffer.Size(instanceMappingRegionIndex[index])
        );
        FixAfterObjectChange(tailIndex, renderObject->objectDataIndex, 1);
    }

    FixAfterObjectChange(index, renderObject->objectDataIndex, 1);
}

void Batch::RemoveObject(RenderObject *renderObject) {
    auto batchIndex = renderObject->batchIndex;
    auto dataIndex = renderObject->objectDataIndex;

    if (renderObjects[batchIndex].size() != 0) {
        totalObjects--;
        renderObjects[batchIndex].erase(renderObjects[batchIndex].begin() + dataIndex);
        instanceMappingBuffer.Erase(
            instanceMappingRegionIndex[batchIndex], sizeof(InstanceMapping), dataIndex * sizeof(InstanceMapping)
        );
        if (objectBuffer.Alignment(batchIndex) != 0)
            objectBuffer.Erase(
                batchIndex, objectBuffer.Alignment(batchIndex), dataIndex * objectBuffer.Alignment(batchIndex)
            );

        uint tailIndex = batchIndex;
        while ((tailIndex = batches[tailIndex].LodPointer()) != 0b11111111111111) {
            instanceMappingBuffer.Erase(
                instanceMappingRegionIndex[tailIndex], sizeof(InstanceMapping), dataIndex * sizeof(InstanceMapping)
            );
            FixAfterObjectChange(batchIndex, dataIndex, -1);
        }
        FixAfterObjectChange(batchIndex, dataIndex, -1);
    }

    if (renderObjects[batchIndex].size() == 0) Remove(batchIndex);
}

void Batch::FixAfterObjectChange(uint batchID, uint firstObject, int objectDelta) {
    if (objectDelta == 0) return;

    uint lastObject = firstObject;
    if (objectDelta > 0) lastObject += objectDelta;

    // Render Objects.
    for (int i = lastObject; i < renderObjects[batchID].size(); i++)
        renderObjects[batchID][i]->objectDataIndex += objectDelta;

    // First Instance.
    for (int i = 0; i < batches.size(); i++) {
        auto &batch = batches[i];
        batch.firstInstance = instanceMappingBuffer.Offset(instanceMappingRegionIndex[i]) /
                              instanceMappingBuffer.Alignment(instanceMappingRegionIndex[i]);
        drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr) batch.firstObject = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        drawCallBuffer.Write(i, batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::FixAfterBatchChange(uint firstBatch, int batchDelta) {
    if (batchDelta == 0) return;

    uint lastBatch = firstBatch;
    if (batchDelta > 0) lastBatch += batchDelta;
    if (lastBatch >= batches.size()) return;

    // Render Objects.
    for (int i = lastBatch; i < renderObjects[lastBatch].size(); i++)
        renderObjects[lastBatch][i]->batchIndex += batchDelta;

    // Lod Pointer and Parent.
    for (int i = 0; i < batches.size(); i++) {
        auto &batch = batches[i];
        if (batch.ParentPointer() != 0b11111111111111 && batch.ParentPointer() > lastBatch)
            batch.SetParentPointer(batch.ParentPointer() + batchDelta);
        if (batch.LodPointer() != 0b11111111111111 && batch.LodPointer() > lastBatch)
            batch.SetLodPointer(batch.LodPointer() + batchDelta);
        drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));

        // First object.
        if (batch.GetLodParent() == nullptr) batch.firstObject = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        drawCallBuffer.Write(i, batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

uint Batch::totalObjects;
std::vector<Batch> Batch::batches;
std::vector<std::tuple<uint16_t, uint16_t>> Batch::materialIndices;
std::vector<uint> Batch::instanceMappingRegionIndex;
RenderBuffer Batch::drawCallBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer});
RenderBuffer Batch::instanceMappingBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer});
std::vector<std::vector<RenderObject *>> Batch::renderObjects;
RenderBuffer Batch::objectBuffer(vg::BufferUsage::StorageBuffer);
