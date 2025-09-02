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

std::tuple<uint, bool> Batch::Add(Material *material, Mesh *mesh, uint objectByteSize) {
    auto [index, exists] = Get(material, mesh);

    if (!exists) {
        assert(index == batches.size() && "TODO: This should insert a new region, not append it");

        Batch batch;
        batch.firstInstance = 0;
        batch.materialIndex =
            Material::materialBuffer.Offset(material->index) / Material::materialBuffer.Alignment(material->index) +
            material->variant;
        batch.meshIndex = mesh->index;
        batch.batchDataElementSize = objectByteSize;
        batch.lodCountPointerParent = NULL_LOD;

        batches.emplace(batches.begin() + index, std::move(batch));
        renderObjects.insert(renderObjects.begin() + index, std::vector<RenderObject *>());
        materialIndices.insert(materialIndices.begin() + index, {material->index, material->variant});

        for (int i = index + 1; i < renderObjects.size(); i++)
            for (auto &&ro : renderObjects[i]) ro->batchIndex++;

        drawCallBuffer.Allocate(sizeof(Batch), sizeof(Batch));
        objectBuffer.Allocate(0, objectByteSize);
        instanceMappingBuffer.Allocate(0, sizeof(InstanceMapping));

        drawCallBuffer.Write(index, batch);
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
    auto recursiveDestroy = [](auto self, uint i) {
        if (i == NULL_LOD) return;
        self(self, i);
        Remove(i);
    };
    recursiveDestroy(recursiveDestroy, batches[index].lodCountPointerParent);
    totalObjects -= renderObjects[index].size();

    batches.erase(batches.begin() + index);
    renderObjects.erase(renderObjects.begin() + index);
    materialIndices.erase(materialIndices.begin() + index);

    drawCallBuffer.Deallocate(index);
    instanceMappingBuffer.Deallocate(index);
    objectBuffer.Deallocate(index);

    for (int i = 0; i < batches.size(); i++) {
        auto &batch = batches[i];
        if (batch.ParentPointer() != 0b11111111111111 && batch.ParentPointer() > index)
            batch.SetParentPointer(batch.ParentPointer() - 1);
        if (batch.LodPointer() != 0b11111111111111 && batch.LodPointer() > index)
            batch.SetLodPointer(batch.LodPointer() - 1);
        drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    }
    for (int i = index; i < batches.size(); i++) {
        batches[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, batches[i].firstInstance, offsetof(Batch, firstInstance));
        for (auto &&ro : renderObjects[i]) ro->batchIndex--;
    }
}

void Batch::ReserveObjects(uint batchIndex, uint objectCount) {
    instanceMappingBuffer.Reserve(objectCount * sizeof(InstanceMapping) * (batches[batchIndex].LodCount() + 1));
    objectBuffer.Reserve(objectCount * objectBuffer.Alignment(batchIndex));
}

void Batch::AddLOD(uint batchIndex, Material *material, Mesh *mesh) {
    auto [index, exists] = Add(material, mesh, batches[batchIndex].batchDataElementSize);
    uint tailIndex = batchIndex;
    for (int i = 0; i < batches[batchIndex].LodCount(); i++) tailIndex = batches[tailIndex].LodPointer();
    batches[tailIndex].SetLodPointer(index);
    batches[batchIndex].SetLodCount(batches[batchIndex].LodCount() + 1);
    batches[index].SetParentPointer(batchIndex);
    drawCallBuffer.Write(tailIndex, batches[tailIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    drawCallBuffer.Write(batchIndex, batches[batchIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    drawCallBuffer.Write(index, batches[index].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    instanceMappingBuffer.Reallocate(index, instanceMappingBuffer.Size(batchIndex));
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
    renderObject->batchIndex = index;
    renderObject->objectDataIndex = 0;
    totalObjects++;
    if (objectByteSize > 0) renderObject->objectDataIndex = objectBuffer.Size(index) / objectBuffer.Alignment(index);

    renderObjects[index].push_back(renderObject);
    objectBuffer.Reallocate(index, objectBuffer.Size(index) + objectByteSize);
    instanceMappingBuffer.Reallocate(index, instanceMappingBuffer.Size(index) + sizeof(InstanceMapping));

    uint tailIndex = index;
    while ((tailIndex = batches[tailIndex].LodPointer()) != 0b11111111111111)
        instanceMappingBuffer.Reallocate(tailIndex, instanceMappingBuffer.Size(index));

    for (int i = 0; i < batches.size(); i++) {
        batches[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, batches[i].firstInstance, offsetof(Batch, firstInstance));
    }
}

void Batch::RemoveObject(RenderObject *renderObject) {
    auto batchIndex = renderObject->batchIndex;
    auto dataIndex = renderObject->objectDataIndex;

    if (renderObjects[batchIndex].size() == 1) Remove(batchIndex);
    else {
        totalObjects--;
        renderObjects[batchIndex].erase(renderObjects[batchIndex].begin() + dataIndex);
        instanceMappingBuffer.Erase(batchIndex, sizeof(InstanceMapping), dataIndex * sizeof(InstanceMapping));
        objectBuffer.Erase(
            batchIndex, objectBuffer.Alignment(batchIndex), dataIndex * objectBuffer.Alignment(batchIndex)
        );
        uint tailIndex = batchIndex;
        while ((tailIndex = batches[tailIndex].LodPointer()) != 0b11111111111111)
            instanceMappingBuffer.Erase(tailIndex, sizeof(InstanceMapping), dataIndex * sizeof(InstanceMapping));

        for (int i = dataIndex; i < renderObjects[batchIndex].size(); i++)
            renderObjects[batchIndex][i]->objectDataIndex--;

        for (int i = 0; i < batches.size(); i++) {
            batches[i].firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
            drawCallBuffer.Write(i, batches[i].firstInstance, offsetof(Batch, firstInstance));
        }
    }
}

uint Batch::totalObjects;
std::vector<Batch> Batch::batches;
std::vector<std::tuple<uint16_t, uint16_t>> Batch::materialIndices;
RenderBuffer Batch::drawCallBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer});
RenderBuffer Batch::instanceMappingBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer});
std::vector<std::vector<RenderObject *>> Batch::renderObjects;
RenderBuffer Batch::objectBuffer(vg::BufferUsage::StorageBuffer);
