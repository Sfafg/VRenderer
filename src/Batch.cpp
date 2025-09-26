#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include <algorithm>

bool Batch::Exists(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    std::tuple key{material, mesh};
    auto it = std::lower_bound(renderer.batches.begin(), renderer.batches.end(), key, [](const auto &a, const auto &b) {
        return a < b;
    });
    return it != renderer.batches.end() && key == *it;
}
uint Batch::Add(Mesh *mesh, Material *material, uint objectByteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    std::tuple key{material, mesh};
    auto it = std::lower_bound(renderer.batches.begin(), renderer.batches.end(), key, [](const auto &a, const auto &b) {
        return a < b;
    });
    bool exists = it != renderer.batches.end() && key == *it;
    uint index = it - renderer.batches.begin();
    if (exists) return index;

    Batch batch;
    if (currentRenderer->materialBuffer.Alignment(material->index) == 0) batch.materialIndex = 0;
    else
        batch.materialIndex = currentRenderer->materialBuffer.Offset(material->index) /
                                  currentRenderer->materialBuffer.Alignment(material->index) +
                              material->variant;
    batch.meshIndex = mesh->index;
    batch.objectDataElementSize = objectByteSize;
    batch.lodCountPointerParent = 0x0FFFFFFF;
    batch.objectCount = 0;

    renderer.drawCallBuffer.Allocate(sizeof(Batch), sizeof(Batch), index);
    renderer.objectBuffer.Allocate(0, objectByteSize, index);
    renderer.instanceMappingBuffer.Allocate(0, sizeof(InstanceMapping), index);

    batch.firstInstance =
        renderer.instanceMappingBuffer.Offset(index) / renderer.instanceMappingBuffer.Alignment(index);
    batch.firstObject = renderer.objectBuffer.Offset(index) / renderer.objectBuffer.Alignment(index);
    renderer.drawCallBuffer.Write(index, batch);

    renderer.batches.emplace(renderer.batches.begin() + index, std::move(batch));
    renderer.renderObjects.insert(renderer.renderObjects.begin() + index, std::vector<RenderObject *>());
    renderer.materialIndices.insert(renderer.materialIndices.begin() + index, {material->index, material->variant});

    for (int i = index + 1; i < renderer.batches.size(); i++) {
        for (int j = 0; j < renderer.renderObjects[i].size(); j++) renderer.renderObjects[i][j]->batchIndex = i;
    }

    // Lod Pointer and Parent.
    for (int i = 0; i < renderer.batches.size(); i++) {
        auto &batch = renderer.batches[i];
        if (batch.ParentPointer() != -1U && batch.ParentPointer() >= index)
            batch.SetParentPointer(batch.ParentPointer() + 1);
        if (batch.LodPointer() != -1U && batch.LodPointer() >= index) batch.SetLodPointer(batch.LodPointer() + 1);
        renderer.drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    }
    return index;
}

uint Batch::Get(Mesh *mesh, Material *material) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    std::tuple key{material, mesh};
    auto it = std::lower_bound(renderer.batches.begin(), renderer.batches.end(), key, [](const auto &a, const auto &b) {
        return a < b;
    });
    uint index = it - renderer.batches.begin();
    return index;
}

void Batch::Remove(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(index < renderer.batches.size() && "Invalid Batch ID.");

    auto remove = [&renderer](uint index) {
        renderer.totalObjects -= renderer.batches[index].objectCount;
        renderer.batches.erase(renderer.batches.begin() + index);
        renderer.renderObjects.erase(renderer.renderObjects.begin() + index);
        renderer.materialIndices.erase(renderer.materialIndices.begin() + index);
        renderer.drawCallBuffer.Deallocate(index);
        renderer.objectBuffer.Deallocate(index);
        renderer.instanceMappingBuffer.Deallocate(index);

        // Lod Pointer and Parent.
        for (int i = 0; i < renderer.batches.size(); i++) {
            auto &batch = renderer.batches[i];
            if (batch.ParentPointer() != -1U && batch.ParentPointer() > index)
                batch.SetParentPointer(batch.ParentPointer() - 1);
            if (batch.LodPointer() != -1U && batch.LodPointer() > index) batch.SetLodPointer(batch.LodPointer() - 1);
            renderer.drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
        }
    };

    uint minIndex = index;
    for (int i = renderer.batches[index].LodCount(); i > 0; i--) {
        uint lodTail = index;
        for (int j = 0; j < i; j++) lodTail = renderer.batches[lodTail].LodPointer();
        remove(lodTail);
        minIndex = std::min(minIndex, lodTail);
        if (index > lodTail) index--;
    }

    remove(index);

    // Render Objects.
    for (int i = minIndex; i < renderer.batches.size(); i++) {
        for (int j = 0; j < renderer.renderObjects[i].size(); j++) renderer.renderObjects[i][j]->batchIndex = i;
    }
    for (int i = minIndex; i < renderer.batches.size(); i++) {
        // First Instance.
        auto &batch = renderer.batches[i];
        batch.firstInstance = renderer.instanceMappingBuffer.Offset(i) / renderer.instanceMappingBuffer.Alignment(i);
        renderer.drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr)
            batch.firstObject = renderer.objectBuffer.Offset(i) / renderer.objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        renderer.drawCallBuffer.Write(i, renderer.batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(batchIndex < renderer.batches.size() && "Invalid Batch ID.");
    assert(renderer.batches[batchIndex].LodCount() == 0 && "Changing LODs has to be implemented.");
    assert(!renderer.batches[batchIndex].IsLOD() && "Cannot assign LOD to LOD batch.");

    uint minIndex = batchIndex;
    for (auto &&[mesh, material] : lods) {
        assert(!Exists(mesh, material) && "Cannot assign already used batch as LOD.");
        uint index = Add(mesh, material, renderer.batches[batchIndex].objectDataElementSize);

        uint tailIndex = batchIndex;
        for (int i = 0; i < renderer.batches[batchIndex].LodCount(); i++)
            tailIndex = renderer.batches[tailIndex].LodPointer();

        renderer.batches[tailIndex].SetLodPointer(index);
        renderer.batches[batchIndex].SetLodCount(renderer.batches[batchIndex].LodCount() + 1);
        renderer.batches[index].SetParentPointer(batchIndex);
        renderer.batches[index].firstObject =
            renderer.objectBuffer.Offset(batchIndex) / renderer.objectBuffer.Alignment(batchIndex);

        renderer.drawCallBuffer.Write(
            tailIndex, renderer.batches[tailIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent)
        );
        renderer.drawCallBuffer.Write(
            batchIndex, renderer.batches[batchIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent)
        );
        renderer.drawCallBuffer.Write(
            index, renderer.batches[index].lodCountPointerParent, offsetof(Batch, lodCountPointerParent)
        );
        renderer.drawCallBuffer.Write(index, renderer.batches[index].firstObject, offsetof(Batch, firstObject));
        renderer.instanceMappingBuffer.Reallocate(index, renderer.instanceMappingBuffer.Size(batchIndex));

        // Lod Pointer and Parent.
        for (int i = 0; i < renderer.batches.size(); i++) {
            auto &batch = renderer.batches[i];
            if (batch.ParentPointer() != -1U && batch.ParentPointer() > index)
                batch.SetParentPointer(batch.ParentPointer() - 1);
            if (batch.LodPointer() != -1U && batch.LodPointer() > index) batch.SetLodPointer(batch.LodPointer() - 1);
            renderer.drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
        }

        minIndex = std::min(minIndex, index);
        if (index <= batchIndex) batchIndex++;
    }

    // Render Objects.
    for (int i = minIndex + 1; i < renderer.batches.size(); i++) {
        for (int j = 0; j < renderer.renderObjects[i].size(); j++) renderer.renderObjects[i][j]->batchIndex = i;
    }
    for (int i = minIndex + 1; i < renderer.batches.size(); i++) {
        // First Instance.
        auto &batch = renderer.batches[i];
        batch.firstInstance = renderer.instanceMappingBuffer.Offset(i) / renderer.instanceMappingBuffer.Alignment(i);
        renderer.drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr)
            batch.firstObject = renderer.objectBuffer.Offset(i) / renderer.objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        renderer.drawCallBuffer.Write(i, renderer.batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::ReserveObjects(uint index, uint objectCount) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(index < renderer.batches.size() && "Invalid Batch ID.");
    if (objectCount <= GetObjectCapacity(index)) return;
    assert(!renderer.batches[index].IsLOD() && "Cannot shrink to fit objects to LOD batch.");

    renderer.renderObjects[index].reserve(objectCount);
    renderer.objectBuffer.Reallocate(index, renderer.objectBuffer.Alignment(index) * objectCount);
    renderer.instanceMappingBuffer.Reallocate(index, sizeof(InstanceMapping) * objectCount);

    uint minIndex = index;
    uint tailIndex = index;
    while ((tailIndex = renderer.batches[tailIndex].LodPointer()) != -1U) {
        renderer.instanceMappingBuffer.Reallocate(tailIndex, renderer.instanceMappingBuffer.Size(index));
        minIndex = std::min(minIndex, tailIndex);
    }

    for (int i = minIndex + 1; i < renderer.batches.size(); i++) {
        // First Instance.
        auto &batch = renderer.batches[i];
        batch.firstInstance = renderer.instanceMappingBuffer.Offset(i) / renderer.instanceMappingBuffer.Alignment(i);
        renderer.drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr)
            batch.firstObject = renderer.objectBuffer.Offset(i) / renderer.objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        renderer.drawCallBuffer.Write(i, renderer.batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::ShrinkToFit(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(index < renderer.batches.size() && "Invalid Batch ID.");
    assert(!renderer.batches[index].IsLOD() && "Cannot shrink to fit objects to LOD batch.");
    uint objectCount = GetObjectCount(index);
    uint objectCapacity = GetObjectCapacity(index);
    if (objectCount == 0) {
        Remove(index);
        return;
    }
    if (objectCount >= objectCapacity) return;
    renderer.renderObjects[index].shrink_to_fit();
    renderer.objectBuffer.Reallocate(index, renderer.objectBuffer.Alignment(index) * objectCount);
    renderer.instanceMappingBuffer.Reallocate(index, sizeof(InstanceMapping) * objectCount);

    uint minIndex = index;
    uint tailIndex = index;
    while ((tailIndex = renderer.batches[tailIndex].LodPointer()) != -1U) {
        renderer.instanceMappingBuffer.Reallocate(tailIndex, renderer.instanceMappingBuffer.Size(index));
        minIndex = std::min(minIndex, tailIndex);
    }

    for (int i = minIndex + 1; i < renderer.batches.size(); i++) {
        // First Instance.
        auto &batch = renderer.batches[i];
        batch.firstInstance = renderer.instanceMappingBuffer.Offset(i) / renderer.instanceMappingBuffer.Alignment(i);
        renderer.drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr)
            batch.firstObject = renderer.objectBuffer.Offset(i) / renderer.objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        renderer.drawCallBuffer.Write(i, renderer.batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

uint Batch::GetObjectCapacity(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(index < renderer.batches.size() && "Invalid Batch ID.");
    return renderer.instanceMappingBuffer.Size(index) / renderer.instanceMappingBuffer.Alignment(index);
}

uint Batch::GetObjectCount(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    assert(index < renderer.batches.size() && "Invalid Batch ID.");
    return renderer.batches[index].objectCount;
}

bool Batch::operator==(const std::tuple<Material *, Mesh *> &o) const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    auto id = this - &renderer.batches[0];
    auto [index, variant] = renderer.materialIndices[id];
    return index == std::get<Material *>(o)->index && variant == std::get<Material *>(o)->variant &&
           meshIndex == std::get<Mesh *>(o)->index;
}

bool Batch::operator<(const std::tuple<Material *, Mesh *> &o) const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    auto id = this - &renderer.batches[0];
    auto [index, variant] = renderer.materialIndices[id];
    if (index < std::get<Material *>(o)->index) return true;
    if (index > std::get<Material *>(o)->index) return false;

    if (variant < std::get<Material *>(o)->variant) return true;
    if (variant > std::get<Material *>(o)->variant) return false;
    return meshIndex < std::get<Mesh *>(o)->index;
}

void Batch::AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    auto index = Add(mesh, material, objectByteSize);
    assert(!renderer.batches[index].IsLOD() && "Cannot add objects to LOD batch.");
    renderObject->batchIndex = index;
    renderObject->objectDataIndex = renderer.renderObjects[index].size();

    if (GetObjectCount(index) + 1 >= GetObjectCapacity(index)) ReserveObjects(index, GetObjectCount(index) + 1);

    renderer.totalObjects++;
    renderer.batches[index].objectCount++;
    renderer.drawCallBuffer.Write(index, renderer.batches[index].objectCount, offsetof(Batch, objectCount));
    renderer.renderObjects[index].push_back(renderObject);
}

void Batch::RemoveObject(RenderObject *renderObject) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    auto index = renderObject->batchIndex;
    auto dataIndex = renderObject->objectDataIndex;

    if (renderer.renderObjects[index].size() > 0) {
        renderer.totalObjects--;
        renderer.batches[index].objectCount--;
        std::swap(
            renderer.renderObjects[index][dataIndex],
            renderer.renderObjects[index][renderer.renderObjects[index].size() - 1]
        );
        renderer.renderObjects[index][dataIndex]->objectDataIndex = dataIndex;
        renderer.renderObjects[index].pop_back();
        // Move object and instance mapping data.
        char *data = new char[renderer.objectBuffer.Alignment(index)];
        renderer.objectBuffer.Read(
            index, data, renderer.objectBuffer.Alignment(index),
            renderer.objectBuffer.Alignment(index) * renderer.batches[index].objectCount
        );
        renderer.objectBuffer.Write(
            index, data, renderer.objectBuffer.Alignment(index), renderer.objectBuffer.Alignment(index) * dataIndex
        );
        delete[] data;
        renderer.drawCallBuffer.Write(index, renderer.batches[index].objectCount, offsetof(Batch, objectCount));
    }

    if (renderer.renderObjects[index].size() == 0) Remove(index);
}

void Batch::NotifyMaterialDestroy(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    for (auto &material : renderer.materialIndices) {
        auto &[matIndex, variant] = material;
        assert(matIndex != index && "Can not destroy material that is being used.");
        if (matIndex > index) {
            matIndex--;
            uint id = &material - &renderer.materialIndices[0];
            renderer.batches[id].materialIndex =
                currentRenderer->materialBuffer.Offset(matIndex) / currentRenderer->materialBuffer.Alignment(matIndex) +
                variant;
            renderer.drawCallBuffer.Write(id, renderer.batches[id].materialIndex, offsetof(Batch, materialIndex));
        }
    }
}

void Batch::NotifyVariantDestroy(uint materialIndex, uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    for (auto &material : renderer.materialIndices) {
        auto &[matIndex, variant] = material;
        if (matIndex != materialIndex) continue;

        assert(variant != index && "Can not destroy material variant that is being used.");
        if (variant > index) {
            variant--;
            uint id = &material - &renderer.materialIndices[0];
            renderer.batches[id].materialIndex =
                currentRenderer->materialBuffer.Offset(matIndex) / currentRenderer->materialBuffer.Alignment(matIndex) +
                variant;
            renderer.drawCallBuffer.Write(id, renderer.batches[id].materialIndex, offsetof(Batch, materialIndex));
        }
    }
}

void Batch::NotifyMeshDestroy(uint index) {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;
    for (auto &batch : renderer.batches) {
        assert(batch.meshIndex != index && "Can not destroy mesh that is being used.");

        if (batch.meshIndex > index) {
            batch.meshIndex--;
            renderer.drawCallBuffer.Write(index, batch.meshIndex, offsetof(Batch, meshIndex));
        }
    }
}

Batch::Batch() {}

uint Batch::LodCount() const { return lodCountPointerParent >> 28 & 0b1111; }

uint Batch::LodPointer() const {
    uint pointer = lodCountPointerParent >> 14 & 0b11111111111111;
    if (pointer == 0b11111111111111) return -1U;
    return pointer;
}

uint Batch::ParentPointer() const {
    uint pointer = lodCountPointerParent & 0b11111111111111;
    if (pointer == 0b11111111111111) return -1U;
    return pointer;
}

void Batch::SetLodCount(uint count) { lodCountPointerParent = (lodCountPointerParent & ~(0b1111 << 28)) | count << 28; }

void Batch::SetLodPointer(uint pointer) {
    lodCountPointerParent = (lodCountPointerParent & ~(0b11111111111111 << 14)) | pointer << 14;
}

void Batch::SetParentPointer(uint pointer) {
    lodCountPointerParent = (lodCountPointerParent & ~0b11111111111111) | pointer;
}

bool Batch::IsLOD() const { return ParentPointer() != -1U; }

Batch *Batch::GetNextLod() const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    auto lod = LodPointer();
    if (lod == -1U) return nullptr;
    assert(lod < renderer.batches.size() && "Invalid Batch ID.");
    return &renderer.batches[lod];
}
Batch *Batch::GetLodParent() const {
    assert(currentRenderer && "Current Renderer needs to be assigned!");
    auto &renderer = *currentRenderer;

    auto lod = ParentPointer();
    if (lod == -1U) return nullptr;
    assert(lod < renderer.batches.size() && "Invalid Batch ID.");
    return &renderer.batches[lod];
}
