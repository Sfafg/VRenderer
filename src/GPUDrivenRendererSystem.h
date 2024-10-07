#pragma once
#include <unordered_map>
#include <span>
#include <glm/gtc/matrix_transform.hpp>
#include "Components.h"
#include "Material.h"
#include "VG/VG.h"
#include "BufferArray.h"
#include "DepthPyramid.h"

class GPUDrivenRendererSystem : public ECS::System<MeshArray>
{
    struct RenderObject
    {
        glm::vec3 boundingCenter;
        float boundingRadius;
        uint32_t objectID;
        uint32_t batchID;
        float padding[2];
        RenderObject(glm::vec3 boundingCenter, float boundingRadius, uint32_t objectID = 0, uint32_t batchID = 0)
            : boundingCenter(boundingCenter), boundingRadius(boundingRadius), objectID(objectID), batchID(batchID)
        {}
    };

    struct MeshReferance
    {
        glm::vec3 boundingCenter;
        float boundingRadius;
        uint32_t vertexStart;
        uint32_t triangleStart;
        uint32_t triangleCount;
        MeshReferance(glm::vec3 boundingCenter, float boundingRadius, uint32_t vertexStart, uint32_t triangleStart, uint32_t triangleCount)
            :boundingCenter(boundingCenter), boundingRadius(boundingRadius), vertexStart(vertexStart), triangleStart(triangleStart), triangleCount(triangleCount)
        {}
    };

    struct Batch
    {
        uint32_t meshID;
        uint32_t objectCount;
        Batch(uint32_t meshID = 0, uint32_t objectCount = 0) :meshID(meshID), objectCount(objectCount) {}
    };

public:
    vg::RenderPass renderPass;

private:
    vg::ComputePipeline computeRenderer;
    vg::DescriptorPool descriptorPool;
    std::vector<vg::DescriptorSet> computeDescriptor;
    std::vector<vg::DescriptorSet> renderDescriptor;

    const vg::Queue* queue;

    BufferArray<uint32_t> indicesBuffer;
    BufferArray<glm::vec3> verticesBuffer;
    std::vector<MeshReferance> meshReferances;

    std::unordered_map<int, int> batchMap;
    std::vector<Batch> batches;
    std::vector<BufferArray<RenderObject>> objectsBuffer;
    std::vector<BufferArray<glm::mat4>> instanceBuffer;
    std::vector<BufferArray<uint32_t>> mappingBuffer;
    std::vector<BufferArray<vg::cmd::DrawIndexed>> instructionsBuffer;
    std::vector<DepthPyramid> depthPyramid;
    uint32_t frameIndex;

public:
    GPUDrivenRendererSystem();
    GPUDrivenRendererSystem(std::span<Material> materials, Span<const vg::Attachment> attachments, uint32_t frameCount, const vg::Queue* queue);

    GPUDrivenRendererSystem(GPUDrivenRendererSystem&& rhs) noexcept;
    GPUDrivenRendererSystem& operator = (GPUDrivenRendererSystem&& rhs);
    GPUDrivenRendererSystem(const GPUDrivenRendererSystem& rhs) = delete;
    GPUDrivenRendererSystem& operator = (const GPUDrivenRendererSystem& rhs) = delete;
    ~GPUDrivenRendererSystem();

    uint32_t AddMesh(Span<const uint32_t> triangles, Span<const glm::vec3> vertices);
    void RemoveMesh();

    void ReserveRenderObjects(int count);
    void AddRenderObject(uint32_t mesh, uint32_t material, const glm::mat4& matrix);
    void RemoveRenderObject();

    void GetRenderCommands(const vg::Image& depthImage, const vg::ImageView& depthView, vg::CmdBuffer& buffer, const glm::mat4& cameraView, const glm::mat4& cameraProjection, const vg::Framebuffer& framebuffer, uint32_t width, uint32_t height);
};