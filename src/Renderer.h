#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Material.h"
#include "Mesh.h"
#include "RenderObject.h"
#include "GPURenderSystem.h"
#include "VG/VG.h"
#include "QueryPool.h"
#include <vector>

class Renderer {
    friend Batch;
    friend RenderBuffer;
    static constexpr uint MAX_FRAMES_IN_FLIGHT = 2;

  public:
    struct PassData {
        glm::mat4 cameraViewProjection;
        glm::mat4 lightViewProjection;
        glm::vec3 cameraPosition;
        float padding1;
        glm::vec3 lightDirection;
        float padding2;
        glm::vec3 lightColor;
        float padding3;
    };

    Renderer();
    Renderer(const vg::Queue *queue, vg::SurfaceHandle windowSurface, int width, int height);
    Renderer(const Renderer &o) = delete;
    Renderer(Renderer &&o) = default;
    Renderer &operator=(const Renderer &o) = delete;
    Renderer &operator=(Renderer &&o) = default;
    ~Renderer();

    void SetPassData(const PassData &data);

    void RenderFrame(const Renderer::PassData &data);
    void Present(vg::Queue &queue);

    // private:
    vg::Surface surface;
    vg::Swapchain swapchain;
    vg::Image depthImage;
    vg::ImageView depthImageView;

    vg::Image shadowImage;
    vg::ImageView shadowImageView;
    vg::Sampler shadowSampler;
    vg::Framebuffer shadowFramebuffer;
    vg::RenderPass shadowPass;

    vg::RenderPass renderPass;
    GPURenderSystem gpuRenderSystem;

    vg::DescriptorPool descriptorPool;
    std::vector<vg::DescriptorSet> descriptorSets;
    std::vector<vg::Framebuffer> framebuffers;
    std::vector<vg::CmdBuffer> commandBuffer;
    std::vector<vg::Semaphore> renderFinishedSemaphore;
    std::vector<vg::Semaphore> imageAvailableSemaphore;
    std::vector<vg::Fence> inFlightFence;
    int frameIndex;
    int presentImageIndex;

    friend Material;
    RenderBuffer materialBuffer;
    std::vector<vg::Subpass> subpasses;
    std::vector<vg::SubpassDependency> dependecies;
    std::vector<std::vector<Material *>> materials;
    void RecreateRenderpass();

    friend Mesh;
    RenderBuffer vertexBuffer;
    RenderBuffer indexBuffer;
    RenderBuffer meshDataBuffer;
    std::vector<Mesh *> meshes;

    friend Batch;
    friend RenderObject;
    friend GPURenderSystem;
    uint totalObjects;
    std::vector<Batch> batches;
    std::vector<std::tuple<uint16_t, uint16_t>> materialIndices;
    std::vector<std::vector<RenderObject *>> renderObjects;
    RenderBuffer instanceMappingBuffer;
    RenderBuffer drawCallBuffer;
    RenderBuffer objectBuffer;

    vg::Buffer passBuffer;
};

extern Renderer *currentRenderer;
