#include "Batch.h"
#include "RenderObject.h"
#include "Mesh.h"
#include "Material.h"
#include <algorithm>

bool Batch::Exists(Mesh *mesh, Material *material) {
    std::tuple key{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), key, [](const auto &a, const auto &b) { return a < b; });
    return it != Batch::batches.end() && key == *it;
}
uint Batch::Add(Mesh *mesh, Material *material, uint objectByteSize) {
    std::tuple key{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), key, [](const auto &a, const auto &b) { return a < b; });
    bool exists = it != Batch::batches.end() && key == *it;
    uint index = it - batches.begin();
    if (exists) return index;

    Batch batch;
    batch.materialIndex =
        Material::materialBuffer.Offset(material->index) / Material::materialBuffer.Alignment(material->index) +
        material->variant;
    batch.meshIndex = mesh->index;
    batch.objectDataElementSize = objectByteSize;
    batch.lodCountPointerParent = 0x0FFFFFFF;
    batch.objectCount = 0;

    drawCallBuffer.Allocate(sizeof(Batch), sizeof(Batch), index);
    objectBuffer.Allocate(0, objectByteSize, index);
    instanceMappingBuffer.Allocate(0, sizeof(InstanceMapping), index);

    batch.firstInstance = instanceMappingBuffer.Offset(index) / instanceMappingBuffer.Alignment(index);
    batch.firstObject = objectBuffer.Offset(index) / objectBuffer.Alignment(index);
    drawCallBuffer.Write(index, batch);

    batches.emplace(batches.begin() + index, std::move(batch));
    renderObjects.insert(renderObjects.begin() + index, std::vector<RenderObject *>());
    materialIndices.insert(materialIndices.begin() + index, {material->index, material->variant});

    for (int i = index + 1; i < batches.size(); i++) {
        for (int j = 0; j < renderObjects[i].size(); j++) renderObjects[i][j]->batchIndex = i;
    }
    for (int i = 0; i < batches.size(); i++) {
        // Lod Pointer and Parent.
        auto &batch = batches[i];
        if (batch.ParentPointer() != 0b11111111111111 && batch.ParentPointer() > index + 1)
            batch.SetParentPointer(batch.ParentPointer() + 1);
        if (batch.LodPointer() != 0b11111111111111 && batch.LodPointer() > index + 1)
            batch.SetLodPointer(batch.LodPointer() + 1);
        drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
    }
    return index;
}

uint Batch::Get(Mesh *mesh, Material *material) {
    std::tuple key{material, mesh};
    auto it = std::lower_bound(batches.begin(), batches.end(), key, [](const auto &a, const auto &b) { return a < b; });
    uint index = it - batches.begin();
    return index;
}

void Batch::Remove(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");

    auto remove = [](uint index) {
        totalObjects -= batches[index].objectCount;
        batches.erase(batches.begin() + index);
        renderObjects.erase(renderObjects.begin() + index);
        materialIndices.erase(materialIndices.begin() + index);
        drawCallBuffer.Deallocate(index);
        objectBuffer.Deallocate(index);
        instanceMappingBuffer.Deallocate(index);

        // Lod Pointer and Parent.
        for (int i = index; i < batches.size(); i++) {
            auto &batch = batches[i];
            if (batch.GetLodParent() && batch.ParentPointer() > index)
                batch.SetParentPointer(batch.ParentPointer() - 1);
            if (batch.GetNextLod() && batch.LodPointer() > index) batch.SetLodPointer(batch.LodPointer() - 1);
            drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
        }
    };

    uint minIndex = index;
    for (int i = batches[index].LodCount(); i > 0; i--) {
        uint lodTail = index;
        for (int j = 0; j < i; j++) lodTail = batches[lodTail].LodPointer();
        remove(lodTail);
        minIndex = std::min(minIndex, lodTail);
        if (index > lodTail) index--;
    }

    remove(index);

    // Render Objects.
    for (int i = minIndex; i < batches.size(); i++) {
        for (int j = 0; j < renderObjects[i].size(); j++) renderObjects[i][j]->batchIndex = i;
    }
    for (int i = minIndex; i < batches.size(); i++) {
        // First Instance.
        auto &batch = batches[i];
        batch.firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr) batch.firstObject = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        drawCallBuffer.Write(i, batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::SetLOD(uint batchIndex, const std::vector<std::tuple<class Mesh *, class Material *>> &lods) {
    assert(batchIndex < batches.size() && "Invalid Batch ID.");
    assert(batches[batchIndex].LodCount() == 0 && "Changing LODs has to be implemented.");
    assert(!batches[batchIndex].IsLOD() && "Cannot assign LOD to LOD batch.");

    uint minIndex = batchIndex;
    for (auto &&[mesh, material] : lods) {
        assert(!Exists(mesh, material) && "Cannot assign already used batch as LOD.");
        uint index = Add(mesh, material, batches[batchIndex].objectDataElementSize);

        uint tailIndex = batchIndex;
        for (int i = 0; i < batches[batchIndex].LodCount(); i++) tailIndex = batches[tailIndex].LodPointer();

        batches[tailIndex].SetLodPointer(index);
        batches[batchIndex].SetLodCount(batches[batchIndex].LodCount() + 1);
        batches[index].SetParentPointer(batchIndex);
        batches[index].firstObject = objectBuffer.Offset(batchIndex) / objectBuffer.Alignment(batchIndex);

        drawCallBuffer.Write(
            tailIndex, batches[tailIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent)
        );
        drawCallBuffer.Write(
            batchIndex, batches[batchIndex].lodCountPointerParent, offsetof(Batch, lodCountPointerParent)
        );
        drawCallBuffer.Write(index, batches[index].lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
        drawCallBuffer.Write(index, batches[index].firstObject, offsetof(Batch, firstObject));
        instanceMappingBuffer.Reallocate(index, instanceMappingBuffer.Size(batchIndex));

        for (int i = index + 1; i < batches.size(); i++) {
            auto &batch = batches[i];
            if (batch.GetLodParent() && batch.ParentPointer() > index)
                batch.SetParentPointer(batch.ParentPointer() - 1);
            if (batch.GetNextLod() && batch.LodPointer() > index) batch.SetLodPointer(batch.LodPointer() - 1);
            drawCallBuffer.Write(i, batch.lodCountPointerParent, offsetof(Batch, lodCountPointerParent));
        }

        minIndex = std::min(minIndex, index);
        if (index <= batchIndex) batchIndex++;
    }

    // Render Objects.
    for (int i = minIndex + 1; i < batches.size(); i++) {
        for (int j = 0; j < renderObjects[i].size(); j++) renderObjects[i][j]->batchIndex = i;
    }
    for (int i = minIndex + 1; i < batches.size(); i++) {
        // First Instance.
        auto &batch = batches[i];
        batch.firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr) batch.firstObject = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        drawCallBuffer.Write(i, batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::ReserveObjects(uint index, uint objectCount) {
    assert(index < batches.size() && "Invalid Batch ID.");
    if (objectCount <= GetObjectCapacity(index)) return;
    assert(!batches[index].IsLOD() && "Cannot shrink to fit objects to LOD batch.");

    renderObjects[index].reserve(objectCount);
    objectBuffer.Reallocate(index, objectBuffer.Alignment(index) * objectCount);
    instanceMappingBuffer.Reallocate(index, sizeof(InstanceMapping) * objectCount);

    uint minIndex = index;
    uint tailIndex = index;
    while ((tailIndex = batches[tailIndex].LodPointer()) != 0b11111111111111) {
        instanceMappingBuffer.Reallocate(tailIndex, instanceMappingBuffer.Size(index));
        minIndex = std::min(minIndex, tailIndex);
    }

    for (int i = minIndex + 1; i < batches.size(); i++) {
        // First Instance.
        auto &batch = batches[i];
        batch.firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr) batch.firstObject = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        drawCallBuffer.Write(i, batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

void Batch::ShrinkToFit(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    assert(!batches[index].IsLOD() && "Cannot shrink to fit objects to LOD batch.");
    uint objectCount = GetObjectCount(index);
    uint objectCapacity = GetObjectCapacity(index);
    if (objectCount == 0) {
        Remove(index);
        return;
    }
    if (objectCount >= objectCapacity) return;
    renderObjects[index].shrink_to_fit();
    objectBuffer.Reallocate(index, objectBuffer.Alignment(index) * objectCount);
    instanceMappingBuffer.Reallocate(index, sizeof(InstanceMapping) * objectCount);

    uint minIndex = index;
    uint tailIndex = index;
    while ((tailIndex = batches[tailIndex].LodPointer()) != 0b11111111111111) {
        instanceMappingBuffer.Reallocate(tailIndex, instanceMappingBuffer.Size(index));
        minIndex = std::min(minIndex, tailIndex);
    }

    for (int i = minIndex + 1; i < batches.size(); i++) {
        // First Instance.
        auto &batch = batches[i];
        batch.firstInstance = instanceMappingBuffer.Offset(i) / instanceMappingBuffer.Alignment(i);
        drawCallBuffer.Write(i, batch.firstInstance, offsetof(Batch, firstInstance));

        // First object.
        if (batch.GetLodParent() == nullptr) batch.firstObject = objectBuffer.Offset(i) / objectBuffer.Alignment(i);
        else batch.firstObject = batch.GetLodParent()->firstObject;
        drawCallBuffer.Write(i, batches[i].firstObject, offsetof(Batch, firstObject));
    }
}

uint Batch::GetObjectCapacity(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return instanceMappingBuffer.Size(index) / instanceMappingBuffer.Alignment(index);
}

uint Batch::GetObjectCount(uint index) {
    assert(index < batches.size() && "Invalid Batch ID.");
    return batches[index].objectCount;
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

void Batch::AddObject(RenderObject *renderObject, Mesh *mesh, Material *material, uint objectByteSize) {
    auto index = Add(mesh, material, objectByteSize);
    assert(!batches[index].IsLOD() && "Cannot add objects to LOD batch.");
    renderObject->batchIndex = index;
    renderObject->objectDataIndex = renderObjects[index].size();

    if (GetObjectCount(index) + 1 >= GetObjectCapacity(index)) ReserveObjects(index, GetObjectCount(index) + 1);

    totalObjects++;
    batches[index].objectCount++;
    drawCallBuffer.Write(index, batches[index].objectCount, offsetof(Batch, objectCount));
    renderObjects[index].push_back(renderObject);
}

void Batch::RemoveObject(RenderObject *renderObject) {
    auto index = renderObject->batchIndex;
    auto dataIndex = renderObject->objectDataIndex;

    if (renderObjects[index].size() > 0) {
        totalObjects--;
        batches[index].objectCount--;
        std::swap(renderObjects[index][dataIndex], renderObjects[index][renderObjects[index].size() - 1]);
        renderObjects[index][dataIndex]->objectDataIndex = dataIndex;
        renderObjects[index].pop_back();
        // Move object and instance mapping data.
        void *data = new char[objectBuffer.Alignment(index)];
        objectBuffer.Read(
            index, data, objectBuffer.Alignment(index), objectBuffer.Alignment(index) * batches[index].objectCount
        );
        objectBuffer.Write(index, data, objectBuffer.Alignment(index), objectBuffer.Alignment(index) * dataIndex);
        delete[] data;
        drawCallBuffer.Write(index, batches[index].objectCount, offsetof(Batch, objectCount));
    }

    if (renderObjects[index].size() == 0) Remove(index);
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

bool Batch::IsLOD() const { return ParentPointer() != 0b11111111111111; }

Batch *Batch::GetNextLod() const {
    auto lod = LodPointer();
    if (lod == 0b11111111111111) return nullptr;
    assert(lod < batches.size() && "Invalid Batch ID.");
    return &batches[lod];
}
Batch *Batch::GetLodParent() const {
    auto lod = ParentPointer();
    if (lod == 0b11111111111111) return nullptr;
    assert(lod < batches.size() && "Invalid Batch ID.");
    return &batches[lod];
}

uint Batch::totalObjects;
std::vector<Batch> Batch::batches;
std::vector<std::tuple<uint16_t, uint16_t>> Batch::materialIndices;
RenderBuffer Batch::drawCallBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer});
RenderBuffer Batch::instanceMappingBuffer({vg::BufferUsage::StorageBuffer, vg::BufferUsage::VertexBuffer});
std::vector<std::vector<RenderObject *>> Batch::renderObjects;
RenderBuffer Batch::objectBuffer(vg::BufferUsage::StorageBuffer);
