#pragma once
#include "Material.h"
#include "Mesh.h"

struct DrawCallArray {

    // I may move it out.
    struct DrawCall {
        uint indexCount = 0;
        uint instanceCount = 0;
        uint firstIndex = 0;
        uint vertexOffset = 0;
        uint firstInstance = 0;
        uint materialDataIndex = 0;
        uint meshIndex = 0;
    };

    struct DrawCallData {
        uint instanceCapacity;
        uint16_t materialIndex;
        uint16_t variantIndex;
    };

    struct DrawCallReferences {
        uint firstInstance = 0;
        uint materialDataIndex = 0;
        uint meshIndex = 0;

        bool operator==(const std::tuple<const DrawCallArray *, const Material *, const Mesh *> &o) const;

        bool operator<(const std::tuple<const DrawCallArray *, const Material *, const Mesh *> &o) const;
    };

    uint transparencyBucketCount = 0;
    uint firstTransparentIndex = 0;
    uint transparentCount = 0;

    std::vector<DrawCallData> drawCallData;
    std::vector<DrawCallReferences> drawCallReferences;
    RenderBuffer drawCallReferencesBuffer;

    DrawCallArray();
    DrawCallArray(int maxFramesInFlight, uint transparencyBucketCount);

    uint GetDrawCall(const Mesh *mesh, const Material *material) const;
    void InsertDrawCall(uint index, const Mesh *mesh, const Material *material);
    void DeleteDrawCall(uint id);

    uint GetInsertionIndex(const Mesh *mesh, const Material *material) const;
    uint GetTotalInstanceCount() const;

  private:
    friend class BatchArray;
    void UpdateFirstInstance(uint firstIndex, uint lastIndex);

    friend Mesh;
    friend Material;
    void NotifyMaterialDestroy(uint index);
    void NotifyVariantDestroy(uint materialIndex, uint index);
    void NotifyMeshDestroy(uint index);
};
