#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include <algorithm>

Batch::Batch() {}

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

        batches.emplace(batches.begin() + index, std::move(batch));
        renderObjects.insert(renderObjects.begin() + index, std::vector<RenderObject *>());
        materialIndices.insert(materialIndices.begin() + index, {material->index, material->variant});

        for (int i = index + 1; i < renderObjects.size(); i++)
            for (auto &&ro : renderObjects[i]) ro->batchIndex++;

        drawCallBuffer.Allocate(sizeof(Batch), sizeof(Batch));
        instanceMappingBuffer.Allocate(0, sizeof(int));
        objectBuffer.Allocate(0, objectByteSize);

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
    batches.erase(batches.begin() + index);
    renderObjects.erase(renderObjects.begin() + index);
    materialIndices.erase(materialIndices.begin() + index);

    drawCallBuffer.Deallocate(index);
    instanceMappingBuffer.Deallocate(index);
    objectBuffer.Deallocate(index);

    for (int i = index; i < renderObjects.size(); i++) {
        batches[index].firstInstance = objectBuffer.Offset(index) / objectBuffer.Alignment(index);
        drawCallBuffer.Write(index, batches[index].firstInstance, offsetof(Batch, firstInstance));

        for (auto &&ro : renderObjects[i]) ro->batchIndex--;
    }
}

void Batch::ReserveObjects(uint batchIndex, uint objectCount) {
    instanceMappingBuffer.Reserve(objectCount * sizeof(uint));
    objectBuffer.Reserve(objectCount * objectBuffer.Alignment(batchIndex));
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
    if (objectByteSize > 0) renderObject->objectDataIndex = objectBuffer.Size(index) / objectBuffer.Alignment(index);

    renderObjects[index].push_back(renderObject);
    instanceMappingBuffer.Reallocate(index, instanceMappingBuffer.Size(index) + sizeof(int));
    objectBuffer.Reallocate(index, objectBuffer.Size(index) + objectByteSize);

    batches[index].firstInstance = objectBuffer.Offset(index) / objectBuffer.Alignment(index);
    drawCallBuffer.Write(index, batches[index].firstInstance, offsetof(Batch, firstInstance));
}

void Batch::RemoveObject(RenderObject *renderObject) {
    auto batchIndex = renderObject->batchIndex;
    auto dataIndex = renderObject->objectDataIndex;

    if (renderObjects[batchIndex].size() == 1) Remove(batchIndex);
    else {
        renderObjects[batchIndex].erase(renderObjects[batchIndex].begin() + dataIndex);
        instanceMappingBuffer.Erase(batchIndex, sizeof(uint), dataIndex * sizeof(uint));
        objectBuffer.Erase(
            batchIndex, objectBuffer.Alignment(batchIndex), dataIndex * objectBuffer.Alignment(batchIndex)
        );

        for (int i = dataIndex; i < renderObjects[batchIndex].size(); i++)
            renderObjects[batchIndex][i]->objectDataIndex--;

        batches[batchIndex].firstInstance = objectBuffer.Offset(batchIndex) / objectBuffer.Alignment(batchIndex);
        drawCallBuffer.Write(batchIndex, batches[batchIndex].firstInstance, offsetof(Batch, firstInstance));
    }
}

std::vector<Batch> Batch::batches;
std::vector<std::tuple<uint16_t, uint16_t>> Batch::materialIndices;
RenderBuffer Batch::drawCallBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer});
RenderBuffer Batch::instanceMappingBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer});
std::vector<std::vector<RenderObject *>> Batch::renderObjects;
RenderBuffer Batch::objectBuffer(vg::BufferUsage::StorageBuffer);
