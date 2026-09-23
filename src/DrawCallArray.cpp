#include "DrawCallArray.h"
#include "Batch.h"
#include <algorithm>
#include <span>

bool DrawCallArray::DrawCallReferences::operator==(
    const std::tuple<const DrawCallArray *, const Material *, const Mesh *> &o
) const {
    auto &[drawCallArray, oMaterial, oMesh] = o;
    uint16_t oMaterialIndex = oMaterial->index;
    uint16_t oVariantIndex = oMaterial->variant;
    uint oMeshIndex = oMesh->index;

    auto id = this - &drawCallArray->drawCallReferences[0];
    uint16_t materialIndex = drawCallArray->drawCallData[id].materialIndex;
    uint16_t variantIndex = drawCallArray->drawCallData[id].variantIndex;

    return materialIndex == oMaterialIndex && variantIndex == oVariantIndex && meshIndex == oMeshIndex;
}

bool DrawCallArray::DrawCallReferences::operator<(
    const std::tuple<const DrawCallArray *, const Material *, const Mesh *> &o
) const {
    auto &[drawCallArray, oMaterial, oMesh] = o;
    uint16_t oMaterialIndex = oMaterial->index;
    uint16_t oVariantIndex = oMaterial->variant;
    uint oMeshIndex = oMesh->index;

    auto id = this - &drawCallArray->drawCallReferences[0];
    uint16_t materialIndex = drawCallArray->drawCallData[id].materialIndex;
    uint16_t variantIndex = drawCallArray->drawCallData[id].variantIndex;

    if (materialIndex < oMaterialIndex) return true;
    if (materialIndex > oMaterialIndex) return false;

    if (variantIndex < oVariantIndex) return true;
    if (variantIndex > oVariantIndex) return false;
    return meshIndex < oMeshIndex;
}

DrawCallArray::DrawCallArray() {}

DrawCallArray::DrawCallArray(int maxFramesInFlight, uint transparencyBucketCount)
    : transparencyBucketCount(transparencyBucketCount) {
    drawCallReferencesBuffer =
        RenderBuffer(maxFramesInFlight, {vg::BufferUsage::StorageBuffer, vg::BufferUsage::IndirectBuffer}, 0);
};

uint DrawCallArray::GetDrawCall(const Mesh *mesh, const Material *material) const {
    std::tuple<const DrawCallArray *, const Material *, const Mesh *> key(this, material, mesh);
    auto it = std::find(drawCallReferences.begin(), drawCallReferences.end(), key);
    if (it == drawCallReferences.end()) return -1U;
    return it - drawCallReferences.begin();
}

void DrawCallArray::InsertDrawCall(uint index, const Mesh *mesh, const Material *material) {
    drawCallReferencesBuffer.Allocate(sizeof(DrawCallReferences), sizeof(DrawCallReferences), index);

    uint previousDrawCallFirstInstance = index > 0 ? drawCallReferences[index - 1].firstInstance : 0;
    uint previousDrawCallInstanceCapacity = index > 0 ? drawCallData[index - 1].instanceCapacity : 0;

    DrawCallReferences drawCall;
    drawCall.firstInstance = previousDrawCallFirstInstance + previousDrawCallInstanceCapacity;
    drawCall.meshIndex = mesh->index;
    drawCall.materialDataIndex = material->GetMaterialDataIndex();
    drawCallReferencesBuffer.Write(index, drawCall);

    drawCallReferences.emplace(drawCallReferences.begin() + index, std::move(drawCall));
    drawCallData.insert(drawCallData.begin() + index, DrawCallData(0, material->index, material->variant));

    if (!material->IsTransparent()) firstTransparentIndex++;

    BatchArray::batchArray->NotifyDrawCallInsert(index);
    UpdateFirstInstance(index + 1, drawCallReferences.size() - 1);

    if (material->IsTransparent() && index < firstTransparentIndex) {
        transparentCount++;
        uint ind = index;
        for (int i = 0; i < transparencyBucketCount - 1; i++) {
            ind += transparentCount;
            InsertDrawCall(ind, mesh, material);
        }
    }
}

void DrawCallArray::DeleteDrawCall(uint id) {
    if (id < firstTransparentIndex) firstTransparentIndex--;
    else if (id < firstTransparentIndex + transparentCount) transparentCount--;

    drawCallReferencesBuffer.Deallocate(id);
    drawCallReferences.erase(drawCallReferences.begin() + id);
    drawCallData.erase(drawCallData.begin() + id);

    BatchArray::batchArray->NotifyDrawCallDestroy(id);

    if (drawCallReferences.size() != 0) UpdateFirstInstance(id, drawCallReferences.size() - 1);
}

uint DrawCallArray::GetInsertionIndex(const Mesh *mesh, const Material *material) const {

    std::tuple<const DrawCallArray *, const Material *, const Mesh *> key(this, material, mesh);
    std::span<const DrawCallReferences> search = {
        drawCallReferences.begin(), drawCallReferences.begin() + firstTransparentIndex
    };

    bool isTransparent = material->IsTransparent();
    if (isTransparent)
        search = {
            drawCallReferences.begin() + firstTransparentIndex,
            drawCallReferences.begin() + firstTransparentIndex + transparentCount
        };

    auto it = std::lower_bound(search.begin(), search.end(), key, [](auto &a, auto &b) { return a < b; });
    return (it - search.begin()) + isTransparent * firstTransparentIndex;
}

uint DrawCallArray::GetTotalInstanceCount() const {
    uint sum;
    for (auto c : drawCallData) sum += c.instanceCapacity;
    return sum;
}

void DrawCallArray::UpdateFirstInstance(uint firstIndex, uint lastIndex) {
    for (int i = firstIndex; i <= lastIndex; i++) {
        if (i == 0) drawCallReferences[i].firstInstance = 0;
        else
            drawCallReferences[i].firstInstance =
                drawCallReferences[i - 1].firstInstance + drawCallData[i - 1].instanceCapacity;
        drawCallReferencesBuffer.Write(
            i, drawCallReferences[i].firstInstance, offsetof(DrawCallReferences, firstInstance)
        );
    }
}

void DrawCallArray::NotifyMaterialDestroy(uint index) {
    assert(Material::materialArray && "Current materialArray needs to be assigned!");

    for (auto &data : drawCallData) {
        const auto &[matIndex, variant] = std::tie(data.materialIndex, data.variantIndex);
        // assert(matIndex != index && "Can not destroy material that is being used.");
        if (matIndex > index) {
            matIndex--;
            uint id = &data - &drawCallData[0];
            drawCallReferences[id].materialDataIndex =
                Material::materialArray->materials[matIndex][variant]->GetMaterialDataIndex();

            drawCallReferencesBuffer.Write(
                id, drawCallReferences[id].materialDataIndex, offsetof(DrawCallReferences, materialDataIndex)
            );
        }
    }
}

void DrawCallArray::NotifyVariantDestroy(uint materialIndex, uint index) {
    assert(Material::materialArray && "Current batchArray needs to be assigned!");

    for (auto &data : drawCallData) {
        const auto &[matIndex, variant] = std::tie(data.materialIndex, data.variantIndex);
        if (matIndex != materialIndex) continue;

        // assert(variant != index && "Can not destroy material variant that is being used.");
        if (variant > index) {
            variant--;
            uint id = &data - &drawCallData[0];
            drawCallReferences[id].materialDataIndex =
                Material::materialArray->materials[matIndex][variant]->GetMaterialDataIndex();

            drawCallReferencesBuffer.Write(
                id, drawCallReferences[id].materialDataIndex, offsetof(DrawCallReferences, materialDataIndex)
            );
        }
    }
}

void DrawCallArray::NotifyMeshDestroy(uint index) {
    for (auto &reference : drawCallReferences) {
        // assert(drawCall.meshIndex != index && "Can not destroy mesh that is being used.");

        if (reference.meshIndex > index) {
            reference.meshIndex--;
            drawCallReferencesBuffer.Write(index, reference.meshIndex, offsetof(DrawCallReferences, meshIndex));
        }
    }
}
